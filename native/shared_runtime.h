// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model2_capture.h"
#include "snapshot_slot.h"
#include "input_policy.h"
#include "game_state.h"
#include "native_weapon.h"
#include "game_options.h"
#include <atomic>
#include <mutex>
namespace gunblade {
struct NativeSnapshot {
    GameState state;
    std::array<NativeAimCalibration,2> calibration;
    ShotOwnership ownership;
};
struct SharedRuntime {
    static constexpr uint32_t Abi=0x47424e05;
    uint32_t abi{Abi};
    SnapshotSlot<const NativeFrame> scene;
    std::mutex mutex;
    GameState state;
    ShotTracker shot_tracker;
    ShotOwnership shot_ownership;
    std::array<NativeAimCalibration,2> calibration;
    std::array<std::shared_ptr<const NativeWeapon>,7> weapons;
    CabinetInput input,engine_input;
    CabinetLatch input_latch;OperatorLatch operator_latch;
    uint64_t session{1};
    GameOptions game_options;
    GameOptionsTransaction game_options_transaction;
    GameOptionRequest game_options_request;
    std::array<GameOptionRequest,4> game_options_queue{};
    unsigned game_options_count{},game_options_index{};uint64_t game_options_next_id{};
    enum class RestartPhase { None, BeforeReset, Saving, AfterReset };
    RestartPhase game_options_restart_phase{RestartPhase::None};
    GameOptions game_options_restart_desired;
    bool game_options_reset_pending{},game_options_reset_dispatched{},game_options_boot_seen{};
    unsigned game_options_restart_ticks{};
    GameOptionsStatus game_options_status{GameOptionsStatus::Idle};GameOptionsError game_options_error{GameOptionsError::None};
    std::atomic<bool> game_options_busy{},game_options_focused{true};
    std::atomic<bool> host_ready{},pause_requested{},quit_requested{},service_requested{},service_active{};
    std::atomic<uint16_t> operator_pending{},operator_latched{};
    bool engine_pause_requested() const{return pause_requested&&!game_options_busy;}
    // The caller owns mutex. Reset requests never retain any cabinet level.
    void clear_inputs_unlocked(){input={};engine_input={};input_latch={};operator_latch={};operator_pending=operator_latched=0;}
    void fail_game_options_restart(GameOptionsError error){
        game_options_restart_phase=RestartPhase::None;game_options_reset_pending=game_options_reset_dispatched=false;
        game_options_request={};game_options_count=game_options_index=0;game_options_status=GameOptionsStatus::Failed;
        game_options_error=error;game_options_busy=false;clear_inputs_unlocked();
    }
    void queue_game_options(const GameOptions& desired){
        game_options_count=game_options_index=0;
        for(auto option:{GameOption::Difficulty,GameOption::ShiftingDifficulty,GameOption::PlayerLife,GameOption::AttractSound})
            if(desired.value(option)!=game_options.value(option))game_options_queue[game_options_count++]={++game_options_next_id,option,desired.value(option)};
        game_options_request=game_options_count?game_options_queue[0]:GameOptionRequest{};
    }
    void request_game_options_reset(RestartPhase phase){
        game_options_restart_phase=phase;game_options_reset_pending=true;game_options_reset_dispatched=game_options_boot_seen=false;
        game_options_restart_ticks=0;game_options_status=GameOptionsStatus::Restarting;game_options_busy=true;clear_inputs_unlocked();
    }
    // Engine thread only. Schedule MAME's reset after this method releases mutex.
    bool take_game_options_reset_request(){
        std::lock_guard lock(mutex);
        if(!game_options_reset_pending)return false;
        if(!game_options_focused||state.service||service_requested||service_active){fail_game_options_restart(GameOptionsError::Interrupted);return false;}
        game_options_reset_pending=false;game_options_reset_dispatched=true;clear_inputs_unlocked();
        ++session;shot_tracker.suspend();shot_ownership={};return true;
    }
    // Emulation thread only; the caller owns mutex (as native_tick does).
    template<class Read32> void tick_game_options(Read32 read,const GameState& state){
    game_options=read_game_options(read);
    if(game_options_restart_phase==RestartPhase::BeforeReset||game_options_restart_phase==RestartPhase::AfterReset){
        if(!game_options_focused||state.service||service_requested||service_active){fail_game_options_restart(GameOptionsError::Interrupted);return;}
        if(!state.valid){fail_game_options_restart(GameOptionsError::InvalidRevision);return;}
        if(++game_options_restart_ticks>1800){fail_game_options_restart(GameOptionsError::Timeout);return;}
        // A scheduled reset is not an acknowledgement. The original program's
        // boot dispatcher must actually run, then return to eligible attract.
        if(game_options_reset_dispatched&&state.mode==11)game_options_boot_seen=true;
        clear_inputs_unlocked();
        if(!game_options_boot_seen||!game_options.valid||!game_options.eligible)return;
        if(game_options_restart_phase==RestartPhase::BeforeReset){
            queue_game_options(game_options_restart_desired);game_options_transaction={};
            if(game_options_count){game_options_restart_phase=RestartPhase::Saving;game_options_status=GameOptionsStatus::Pending;}
            else request_game_options_reset(RestartPhase::AfterReset);
        }else {
            for(auto option:{GameOption::Difficulty,GameOption::ShiftingDifficulty,GameOption::PlayerLife,GameOption::AttractSound})
                if(game_options.value(option)!=game_options_restart_desired.value(option)){fail_game_options_restart(GameOptionsError::NativeMismatch);return;}
            game_options_restart_phase=RestartPhase::None;game_options_request={};game_options_busy=false;
            game_options_status=GameOptionsStatus::Complete;game_options_error=GameOptionsError::None;
        }
        return;
    }
    // Restart failures/completion are terminal until a new explicit request;
    // leave manual Test/Service pulses available for native save recovery.
    if(!game_options_request.id&&!game_options_transaction.busy()){
        operator_latched=operator_latch.update(operator_pending.exchange(0));return;
    }
    const auto previous_options_status=game_options_status;
    uint16_t option_buttons=game_options_transaction.tick(read,state,game_options_request,game_options_focused);
    game_options_status=game_options_transaction.status();game_options_error=game_options_transaction.error();
    if(game_options_status==GameOptionsStatus::Complete&&game_options_index+1<game_options_count){
        game_options_request=game_options_queue[++game_options_index];game_options_status=GameOptionsStatus::Pending;game_options_busy=true;
    }else if(game_options_status==GameOptionsStatus::Complete&&game_options_restart_phase==RestartPhase::Saving){
        request_game_options_reset(RestartPhase::AfterReset);return;
    }else game_options_busy=game_options_transaction.busy();
    if(game_options_status==GameOptionsStatus::Failed&&game_options_restart_phase==RestartPhase::Saving){
        fail_game_options_restart(game_options_error);return;
    }
    if(game_options_status==GameOptionsStatus::Failed&&previous_options_status!=GameOptionsStatus::Failed){game_options_count=0;operator_latch={};operator_latched=0;operator_pending=0;input={};engine_input={};input_latch={};}
    operator_latched=operator_latch.update(option_buttons|operator_pending.exchange(0));
    }
    NativeSnapshot native_snapshot(){std::lock_guard lock(mutex);return {state,calibration,shot_ownership};}
    bool begin_game_options_restart(const GameOptions& desired){
        std::lock_guard lock(mutex);
        if(game_options_busy||!game_options.valid||!state.valid||state.service||service_requested||service_active||!game_options_focused)return false;
        bool changed=false;
        for(auto option:{GameOption::Difficulty,GameOption::ShiftingDifficulty,GameOption::PlayerLife,GameOption::AttractSound}){
            if(!game_option_value_valid(option,desired.value(option)))return false;
            changed|=desired.value(option)!=game_options.value(option);
        }
        if(!changed)return false;
        game_options_restart_desired=desired;game_options_count=game_options_index=0;game_options_request={};game_options_transaction={};
        game_options_error=GameOptionsError::None;request_game_options_reset(RestartPhase::BeforeReset);return true;
    }
    bool begin_game_options(const GameOptions& desired){
        std::lock_guard lock(mutex);
        if(game_options_busy||!game_options.valid||!game_options.eligible)return false;
        game_options_count=game_options_index=0;
        for(auto option:{GameOption::Difficulty,GameOption::ShiftingDifficulty,GameOption::PlayerLife,GameOption::AttractSound}){
            if(!game_option_value_valid(option,desired.value(option)))return false;
            if(desired.value(option)!=game_options.value(option))game_options_queue[game_options_count++]={++game_options_next_id,option,desired.value(option)};
        }
        if(!game_options_count)return false;
        input={};engine_input={};input_latch={};operator_latch={};operator_pending=operator_latched=0;
        game_options_restart_phase=RestartPhase::None;
        game_options_request=game_options_queue[0];game_options_status=GameOptionsStatus::Pending;game_options_error=GameOptionsError::None;game_options_busy=true;return true;
    }
    void release_inputs(){std::lock_guard lock(mutex);input={};engine_input={};input_latch={};operator_latch={};operator_pending=0;operator_latched=0;}
    void reset(){scene.store({});weapons={};{std::lock_guard lock(mutex);state={};calibration={};shot_tracker.suspend();shot_ownership={};input={};engine_input={};input_latch={};operator_latch={};game_options={};game_options_transaction={};game_options_request={};game_options_count=game_options_index=0;game_options_next_id=0;game_options_restart_phase=RestartPhase::None;game_options_restart_desired={};game_options_reset_pending=game_options_reset_dispatched=game_options_boot_seen=false;game_options_restart_ticks=0;game_options_status=GameOptionsStatus::Idle;game_options_error=GameOptionsError::None;++session;}game_options_busy=false;game_options_focused=true;host_ready=false;pause_requested=false;quit_requested=false;service_requested=false;service_active=false;operator_pending=0;operator_latched=0;}
};
inline SharedRuntime& shared_runtime(){static SharedRuntime runtime;return runtime;}
}
