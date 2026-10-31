// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "cabinet_io.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
namespace gunblade {
struct PlayerState {
    uint32_t state{},ammo{},capacity{},shots{},weapon{};
    float health{};
    bool joined() const { return state != 0; }
    bool alive() const { return joined() && health > 0; }
};
struct GameState {
    bool valid{},ready{},playing{},counting_shots{},service{},gun_calibration{};
    bool shot_counters_valid{};
    uint32_t mode{},submode{},service_menu{},service_submode{},service_busy{};
    uint64_t session{};
    std::array<PlayerState,2> players{};
};
// Physical i960 program addresses for the audited Gunblade NY Revision A.
// These six words identify the dispatcher, join handler, life gate and AIM SET
// handler. A matching MAME build alone does not identify the loaded game ROM.
template<class Read32> GameState read_game_state(Read32 read) {
    GameState out;
    constexpr std::array<std::array<uint32_t,2>,6> fingerprint{{
        {0x075d64,0x8c203000},{0x075d68,0x00532d54},
        {0x0ed8f8,0x58a50980},{0x06d414,0x90a16014},
        {0x0f0bb0,0x0054bdc4},{0x0f1bcc,0x000f0ba0}
    }};
    for(const auto& word:fingerprint)if(read(word[0])!=word[1])return out;
    out.valid=true;
    out.mode=read(0x532d54);out.submode=read(0x532d48);
    out.ready=out.mode==0 || out.mode==1 || out.mode==8 || out.mode==9 ||
              (out.mode==2 && read(0x5401d0)==0);
    out.playing=out.mode==2; // Includes native selection and stage introduction.
    out.service=out.mode==10;
    if(out.service) {
        out.service_menu=read(0x54bdf8);
        out.gun_calibration=out.submode==3 && out.service_menu==7;
        if(out.gun_calibration) {
            // Each player calibrates independently: 0 idle, 1 initialize, 2 scan.
            const auto p1=read(0x54bdc4),p2=read(0x54bdc8);
            out.service_submode=(p1&0xff) | ((p2&0xff)<<8);
            if(p1>0 && p1<=2)out.service_busy|=1;
            if(p2>0 && p2<=2)out.service_busy|=2;
        }
    }
    if(out.playing) {
        const auto joined=read(0x532d58)&3;
        const auto initial_lives=read(0x5329f0);
        for(unsigned player=0;player<2;player++) {
            auto& state=out.players[player];
            state.state=(joined>>player)&1;
            const auto lives=read(0x540114+0x40*player);
            // Invalid/negative data cannot make a dead player appear alive.
            if(state.joined() && initial_lives>0 && initial_lives<=255 && lives<=initial_lives)
                state.health=static_cast<float>(lives)/static_cast<float>(initial_lives);
        }
    }
    // No ammunition/reload or weapon switching is invented for the machinegun.
    // Trigger mirrors, held-frame clocks and an eight-slot projectile cursor are
    // not cumulative per-player shots. Feedback stays off without a tracker.
    return out;
}
class ShotTracker {
    std::array<uint32_t,2> shots_{};
    uint64_t session_{};
    uint32_t frame_{},cursor_{},joined_{};
    bool initialized_{},trusted_{};
public:
    void suspend() { *this=ShotTracker{}; }
    // Call on the emulation thread after reading state and assigning its session,
    // once per native vblank. Skipped/reset frames are deliberately not counted.
    template<class Read32> void update(GameState& state,Read32 read) {
        state.counting_shots=state.shot_counters_valid=false;
        if(!state.valid || session_!=state.session) {
            suspend();session_=state.session;
        }
        for(unsigned player=0;player<2;player++)state.players[player].shots=shots_[player];
        const auto handler=state.valid?read(0x532964):0;
        const uint32_t joined=(state.players[0].joined()?1u:0u) | (state.players[1].joined()?2u:0u);
        if(!state.valid || !state.playing || state.submode!=2 || !joined ||
           (handler!=0x71110 && handler!=0x71440 && handler!=0x71770)) {
            initialized_=trusted_=false;return;
        }
        const auto frame=read(0x540088),cursor=read(0x5329ac);
        if(cursor>=8) { initialized_=trusted_=false;return; }
        if(!initialized_ || joined!=joined_) {
            frame_=frame;cursor_=cursor;joined_=joined;initialized_=true;trusted_=false;return;
        }
        const auto elapsed=frame-frame_;
        const auto count=(cursor-cursor_)&7;
        bool accepted=elapsed==1 && count<=2;
        std::array<uint32_t,2> delta{};
        if(accepted)for(uint32_t i=0;i<count;i++) {
            const auto slot=(cursor_+i)&7;
            const auto owner=read(0x532a40+0x48*slot+0x2c);
            if(owner>1 || !(joined&(1u<<owner)) || delta[owner]) {
                accepted=false;break;
            }
            delta[owner]=1;
        }
        // A second host read of the same native frame preserves validity but
        // cannot replay its events. An inconsistent same-frame cursor is rejected.
        if(elapsed==0 && count==0)accepted=trusted_;
        else if(accepted)for(unsigned player=0;player<2;player++)shots_[player]+=delta[player];
        frame_=frame;cursor_=cursor;trusted_=accepted;
        state.counting_shots=state.shot_counters_valid=accepted;
        for(unsigned player=0;player<2;player++)state.players[player].shots=shots_[player];
    }
};
struct NativeAimCalibration {
    bool valid{},factory{},screen_inverse_valid{};
    uint8_t left{},right{},top{},bottom{},center_x{},center_y{};
    float angle_units_x{},angle_units_y{},origin_x{},origin_y{};
};
// Read the game's committed ADC calibration, not the cabinet input limits. The
// factory vertical span is negative and the native center is not necessarily
// the midpoint. The inverse below targets the audited gameplay reticle window.
template<class Read32> NativeAimCalibration read_native_calibration(Read32 read,unsigned player) {
    NativeAimCalibration out;
    if(player>1 || !read_game_state(read).valid)return out;
    const auto byte=[&](uint32_t address) {
        return static_cast<uint8_t>(read(address&~3u)>>((address&3)*8));
    };
    if(player==0) {
        out.left=byte(0x54c0f1);out.right=byte(0x54c0f2);
        out.top=byte(0x54c0f0);out.bottom=byte(0x54c0ef);
        out.center_x=byte(0x54c0f3);out.center_y=byte(0x54c0f4);
    } else {
        out.left=byte(0x54c102);out.right=byte(0x54c103);
        out.top=byte(0x54c101);out.bottom=byte(0x54c100);
        out.center_x=byte(0x54c104);out.center_y=byte(0x54c105);
    }
    out.origin_x=std::bit_cast<float>(read(player==0?0x54c0f8:0x54c108));
    out.origin_y=std::bit_cast<float>(read(player==0?0x54c0fc:0x54c10c));
    if(out.left==out.right || out.top==out.bottom ||
       !std::isfinite(out.origin_x) || !std::isfinite(out.origin_y))return {};
    // 0x70ff0..0x71038: native angular gain before center subtraction and TGP.
    out.angle_units_x=(37.5f/(int(out.right)-int(out.left)))*183.0f;
    out.angle_units_y=(27.0f/(int(out.bottom)-int(out.top)))*183.0f;
    out.factory=out.left==64 && out.right==192 && out.top==192 && out.bottom==64 &&
                out.center_x==128 && out.center_y==128 &&
                out.origin_x==(player==0?-3.2f:3.2f) && out.origin_y==-1.5f;
    out.valid=out.screen_inverse_valid=true;return out;
}
struct NativeAimPoint { double x{},y{},z{};bool valid{}; };
inline double native_aim_axis(uint8_t raw,uint8_t center,float gain,float origin) {
    // Original i960 float multiply, truncate, low-halfword FIFO write, then
    // original SHARC command 29. Bounded native ADC sweeps verify pi/32767.
    const auto integer=static_cast<int32_t>(static_cast<float>(int(raw)-int(center))*gain);
    const auto angle=static_cast<int16_t>(static_cast<uint16_t>(integer));
    return double(origin)+13.0*std::sin(double(angle)*(3.14159265358979323846/32767.0));
}
inline NativeAimPoint native_aim_point(const NativeAimCalibration& calibration,uint8_t x,uint8_t y) {
    if(!calibration.valid || !std::isfinite(calibration.angle_units_x) ||
       !std::isfinite(calibration.angle_units_y) || !std::isfinite(calibration.origin_x) ||
       !std::isfinite(calibration.origin_y) || calibration.angle_units_x==0 ||
       calibration.angle_units_y==0 || std::abs(calibration.angle_units_x)>6862.5f ||
       std::abs(calibration.angle_units_y)>4941.0f)return {};
    return {native_aim_axis(x,calibration.center_x,calibration.angle_units_x,calibration.origin_x),
            native_aim_axis(y,calibration.center_y,calibration.angle_units_y,calibration.origin_y),10.0,true};
}
inline AimValue native_screen_to_adc(unsigned player,double u,double v,const NativeAimCalibration& calibration) {
    if(player>1 || !calibration.valid || !calibration.screen_inverse_valid ||
       !std::isfinite(u) || !std::isfinite(v) ||
       !native_aim_point(calibration,calibration.center_x,calibration.center_y).valid)return {};
    // Native gameplay reticle window: 600/600 focus, depth 10, visible center
    // (248,192), raster 496x384. Scene/world focus 400 is a different window.
    const auto closest=[](double target,int first,int last,auto project) {
        int best=first;double distance=INFINITY;
        for(int value=first;value<=last;value++) {
            const auto error=std::abs(project(static_cast<uint8_t>(value))-target);
            if(error<distance) { distance=error;best=value; }
        }
        return static_cast<uint8_t>(best);
    };
    const auto& limits=NativeRanges[player];
    // NativeLens uses inclusive raster endpoints: pixel/(dimension-1).
    const auto x=closest(std::clamp(u,0.0,1.0)*495.0,limits.left,limits.right,[&](uint8_t raw) {
        return 248.0+60.0*native_aim_point(calibration,raw,calibration.center_y).x;
    });
    const auto y=closest(std::clamp(v,0.0,1.0)*383.0,limits.top,limits.bottom,[&](uint8_t raw) {
        return 192.0-60.0*native_aim_point(calibration,calibration.center_x,raw).y;
    });
    return {x,y,true};
}
}
