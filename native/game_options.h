// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "game_state.h"
#include <array>
#include <cstdint>
namespace gunblade {
enum class GameOption { Difficulty, ShiftingDifficulty, PlayerLife, AttractSound };
struct GameOptionRequest { uint64_t id{};GameOption option{GameOption::Difficulty};uint32_t value{}; };
enum class GameOptionsStatus { Idle, Pending, Saving, Complete, Failed, Restarting };
enum class GameOptionsError { None, InvalidRevision, InvalidValue, Unavailable, Interrupted, Timeout, NativeMismatch };
struct GameOptions {
    bool valid{},eligible{};
    uint32_t difficulty{},shifting_difficulty{},player_life{},attract_sound{};
    uint32_t value(GameOption option) const {
        switch(option){case GameOption::Difficulty:return difficulty;case GameOption::ShiftingDifficulty:return shifting_difficulty;
        case GameOption::PlayerLife:return player_life;case GameOption::AttractSound:return attract_sound;}return 0;
    }
};
inline bool game_option_value_valid(GameOption option,uint32_t value) {
    switch(option){case GameOption::Difficulty:case GameOption::ShiftingDifficulty:return value<=7;
    case GameOption::PlayerLife:return value>=1&&value<=7;case GameOption::AttractSound:return value<=1;}return false;
}
inline uint32_t game_option_address(GameOption option) {
    switch(option){case GameOption::Difficulty:return 0x54c0e9;case GameOption::ShiftingDifficulty:return 0x54c0e7;
    case GameOption::PlayerLife:return 0x54c0ea;case GameOption::AttractSound:return 0x54c0e4;}return 0;
}
template<class Read32> GameOptions read_game_options(Read32 read) {
    GameOptions out;
    // Additional guards cover the actual assignment reader, life writer and
    // original EEPROM save dispatch, beyond read_game_state's revision guard.
    constexpr std::array<std::array<uint32_t,2>,6> fingerprint{{
        {0x0efc10,0x8c086140},{0x0efca4,0x0054c0e4},{0x0f01bc,0x0054c0ea},
        {0x0f1ae8,0x09f1c348},{0x00de30,0x90a03000},{0x00de34,0x0054c0dc}
    }};
    const auto state=read_game_state(read);if(!state.valid)return out;
    for(const auto& word:fingerprint)if(read(word[0])!=word[1])return out;
    if(read(0x54c0d0)!=0x41474553||read(0x54c0d4)!=0x594e4247||(read(0x54c0e0)&0xffff)!=0x58)return out;
    auto byte=[&](uint32_t a){return (read(a&~3u)>>((a&3)*8))&255u;};
    out.difficulty=byte(0x54c0e9);out.shifting_difficulty=byte(0x54c0e7);
    out.player_life=byte(0x54c0ea);out.attract_sound=byte(0x54c0e4);
    if(out.difficulty>7||out.shifting_difficulty>7||out.player_life<1||out.player_life>7||out.attract_sound>1)return {};
    out.valid=true;
    out.eligible=(state.mode==0||state.mode==1||state.mode==8||state.mode==9)&&!(read(0x532d58)&3);
    return out;
}
// This object belongs exclusively to the emulation thread, once per vblank.
// Return values are edge requests for OperatorLatch (which holds12 frames).
// Never write RAM, EEPROM or native CPU registers through this adapter.
class GameOptionsTransaction {
    enum class Phase { Enter, Assignment, Change, LeaveAssignment, ParentExit, Commit };
    GameOptionRequest request_{};
    GameOptionsStatus status_{GameOptionsStatus::Idle};GameOptionsError error_{};
    Phase phase_{};unsigned ticks_{},cooldown_{};
    uint32_t writes_{};std::array<uint8_t,0x58> baseline_{};
    uint16_t fail(GameOptionsError error){status_=GameOptionsStatus::Failed;error_=error;return 0;}
    uint16_t pulse(uint16_t button){cooldown_=24;return button;}
    unsigned row() const {
        switch(request_.option){case GameOption::Difficulty:return 3;case GameOption::ShiftingDifficulty:return 4;
        case GameOption::PlayerLife:return 5;case GameOption::AttractSound:return 0;}return 8;
    }
public:
    bool busy() const{return status_==GameOptionsStatus::Pending||status_==GameOptionsStatus::Saving;}
    bool native_service_owned() const{return status_==GameOptionsStatus::Saving;}
    GameOptionsStatus status() const{return status_;}
    GameOptionsError error() const{return error_;}
    uint64_t request_id() const{return request_.id;}
    template<class Read32> uint16_t tick(Read32 read,const GameState& state,const GameOptionRequest& next,bool focused=true) {
        const auto options=read_game_options(read);
        auto byte=[&](uint32_t a){return static_cast<uint8_t>(read(a&~3u)>>((a&3)*8));};
        if(!busy()) {
            if(!next.id||next.id==request_.id)return 0;
            request_=next;error_=GameOptionsError::None;ticks_=cooldown_=0;
            if(!options.valid)return fail(GameOptionsError::InvalidRevision);
            if(!game_option_value_valid(next.option,next.value))return fail(GameOptionsError::InvalidValue);
            if(!focused)return fail(GameOptionsError::Interrupted);
            if(!options.eligible)return fail(GameOptionsError::Unavailable);
            if(options.value(next.option)==next.value){status_=GameOptionsStatus::Complete;return 0;}
            for(unsigned i=0;i<baseline_.size();i++)baseline_[i]=byte(0x54c0d0+i);
            writes_=read(0x54c0dc);phase_=Phase::Enter;status_=GameOptionsStatus::Saving;
        }
        if(!focused)return fail(GameOptionsError::Interrupted);
        if(!options.valid||!state.valid)return fail(GameOptionsError::InvalidRevision);
        if(++ticks_>1800)return fail(GameOptionsError::Timeout);
        if(state.playing||(read(0x532d58)&3))return fail(GameOptionsError::Interrupted);
        if(cooldown_){--cooldown_;return 0;}
        switch(phase_) {
        case Phase::Enter:
            if(!state.service)return pulse(4);
            phase_=Phase::Assignment;[[fallthrough]];
        case Phase::Assignment:
            if(!state.service)return fail(GameOptionsError::Interrupted);
            if(state.submode!=1)return 0;
            if(state.service_menu!=5)return pulse(8);
            phase_=Phase::Change;return pulse(4);
        case Phase::Change:
            if(!state.service)return fail(GameOptionsError::Interrupted);
            if(state.submode!=3)return 0;
            if(state.service_menu!=5)return fail(GameOptionsError::NativeMismatch);
            if(read(0x54bda4)!=row())return pulse(8);
            if(options.value(request_.option)!=request_.value)return pulse(4);
            phase_=Phase::LeaveAssignment;[[fallthrough]];
        case Phase::LeaveAssignment:
            if(!state.service||state.service_menu!=5)return fail(GameOptionsError::NativeMismatch);
            if(state.submode!=3)return 0;
            if(read(0x54bda4)!=8)return pulse(8);
            phase_=Phase::ParentExit;return pulse(4);
        case Phase::ParentExit:
            if(!state.service)return fail(GameOptionsError::Interrupted);
            if(state.submode!=1)return 0;
            if(state.service_menu!=10)return pulse(8);
            phase_=Phase::Commit;return pulse(4);
        case Phase::Commit:
            if(state.service)return 0;
            if(!options.eligible||options.value(request_.option)!=request_.value)return fail(GameOptionsError::NativeMismatch);
            // Native Exit synchronously calls checksum + serial EEPROM save
            // before returning to attract. Readback alone is not a save ack.
            if(read(0x54c0dc)!=writes_+1)return 0;
            for(unsigned i=0;i<baseline_.size();i++) {
                if(i==8||i==9||(i>=12&&i<16)||0x54c0d0+i==game_option_address(request_.option))continue;
                if(byte(0x54c0d0+i)!=baseline_[i])return fail(GameOptionsError::NativeMismatch);
            }
            status_=GameOptionsStatus::Complete;return 0;
        }
        return 0;
    }
};
}
