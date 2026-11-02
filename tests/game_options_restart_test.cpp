// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/shared_runtime.h"
#include <cassert>
#include <iostream>
#include <unordered_map>
using namespace gunblade;
struct Memory {
    std::unordered_map<uint32_t,uint32_t> words{
        {0x075d64,0x8c203000},{0x075d68,0x00532d54},
        {0x0ed8f8,0x58a50980},{0x06d414,0x90a16014},
        {0x0f0bb0,0x0054bdc4},{0x0f1bcc,0x000f0ba0},
        {0x0efc10,0x8c086140},{0x0efca4,0x0054c0e4},
        {0x0f01bc,0x0054c0ea},{0x0f1ae8,0x09f1c348},
        {0x00de30,0x90a03000},{0x00de34,0x0054c0dc},
        {0x54c0d0,0x41474553},{0x54c0d4,0x594e4247},
        {0x54c0e0,0xffff0058},{0x54c0e4,0x03000001},
        {0x54c0e8,0x000303ff},{0x54c0ec,0x40000101},
        {0x54bdf8,10},{0x54bda4,8}
    };
    uint32_t operator()(uint32_t a) const {assert(!(a&3));auto i=words.find(a);return i==words.end()?0:i->second;}
    void byte(uint32_t a,uint32_t v){auto& w=words[a&~3u];auto s=(a&3)*8;w=(w&~(255u<<s))|(v<<s);}
    void mode(uint32_t mode,uint32_t joined=0){words[0x532d54]=mode;words[0x532d58]=joined;}
    void tick(SharedRuntime& r){r.tick_game_options(*this,read_game_state(*this));r.state=read_game_state(*this);}
    void initialize(SharedRuntime& r){r.state=read_game_state(*this);r.game_options=read_game_options(*this);r.pause_requested=true;}
};
GameOptions desired(const SharedRuntime& r){auto d=r.game_options;d.difficulty=5;return d;}
// Removing the pre-reset or starting Test during a round must fail this test.
void active_round_requires_real_boot_before_service() {
    Memory m;SharedRuntime r;m.mode(2,3);m.initialize(r);
    r.input.cabinet.starts=r.engine_input.cabinet.starts=3;r.operator_latched=8;
    assert(r.begin_game_options_restart(desired(r)));
    assert(r.game_options_busy&&!r.engine_pause_requested());
    assert(!r.input.cabinet.starts&&!r.engine_input.cabinet.starts&&!r.operator_latched);
    assert(!r.begin_game_options_restart(desired(r)));
    m.tick(r);assert(r.game_options_status==GameOptionsStatus::Restarting&&!r.operator_latched);
    assert(r.take_game_options_reset_request());assert(!r.take_game_options_reset_request());
    m.mode(0);for(unsigned i=0;i<30;i++)m.tick(r);
    assert(r.game_options_status==GameOptionsStatus::Restarting&&!r.operator_latched);
    m.mode(11);m.tick(r);m.mode(0);m.tick(r);
    assert(r.game_options_busy&&r.game_options_status==GameOptionsStatus::Pending);
    m.tick(r);assert(r.operator_latched==4&&r.game_options_status==GameOptionsStatus::Saving);
}
void manual_service_and_untrusted_requests_never_reset() {
    for(unsigned kind=0;kind<6;kind++) {
        Memory m;SharedRuntime r;m.initialize(r);auto d=desired(r);
        if(kind==0){m.mode(10);m.initialize(r);}
        if(kind==1)r.service_requested=true;
        if(kind==2)r.service_active=true;
        if(kind==3)r.game_options_focused=false;
        if(kind==4)r.game_options.valid=false;
        if(kind==5)d.player_life=0;
        assert(!r.begin_game_options_restart(d));assert(!r.take_game_options_reset_request());
    }
    Memory m;SharedRuntime r;m.initialize(r);
    assert(r.begin_game_options(desired(r)));assert(!r.begin_game_options_restart(desired(r)));
    assert(!r.take_game_options_reset_request());
}
void focus_loss_timeout_and_manual_service_cancel_pending_reset() {
    for(unsigned kind=0;kind<3;kind++) {
        Memory m;SharedRuntime r;m.mode(2,1);m.initialize(r);
        assert(r.begin_game_options_restart(desired(r)));
        if(kind==0)r.game_options_focused=false;
        if(kind==1){assert(r.take_game_options_reset_request());for(unsigned i=0;i<1801;i++)m.tick(r);}
        if(kind==2){m.mode(10);}
        m.tick(r);
        assert(r.game_options_status==GameOptionsStatus::Failed&&!r.game_options_busy);
        assert(r.game_options_error==(kind==1?GameOptionsError::Timeout:GameOptionsError::Interrupted));
        assert(!r.take_game_options_reset_request()&&!r.operator_latched);
        // Failure clears once; original manual Service remains usable.
        r.game_options_focused=true;r.operator_pending=8;m.tick(r);assert(r.operator_latched==8);
    }
}
void failure_during_original_save_releases_inputs_without_final_reset() {
    Memory m;SharedRuntime r;m.mode(2,1);m.initialize(r);
    assert(r.begin_game_options_restart(desired(r)));assert(r.take_game_options_reset_request());
    m.mode(11);m.tick(r);m.mode(0);m.tick(r);m.tick(r);
    assert(r.operator_latched==4);
    m.mode(10);m.words[0x532d48]=1;r.game_options_focused=false;r.operator_pending=8;m.tick(r);
    assert(r.game_options_status==GameOptionsStatus::Failed&&!r.game_options_busy);
    assert(r.game_options_error==GameOptionsError::Interrupted&&!r.operator_latched&&!r.operator_pending);
    assert(!r.take_game_options_reset_request());
    r.game_options_focused=true;r.operator_pending=8;m.tick(r);assert(r.operator_latched==8);
    r.reset();assert(!r.game_options_busy&&!r.take_game_options_reset_request());
    assert(r.game_options_status==GameOptionsStatus::Idle);
}
// Drive the actual production transaction through the independently observed
// native phases. A value change without the EEPROM write ACK cannot restart.
void save_requires_ack_and_final_boot_readback(bool mismatch=false,bool corrupt_calibration=false) {
    Memory m;SharedRuntime r;m.mode(2,1);m.initialize(r);
    assert(r.begin_game_options_restart(desired(r)));assert(r.take_game_options_reset_request());
    m.mode(11);m.tick(r);m.mode(0);m.tick(r);m.tick(r);
    auto pulse=[&](uint16_t want){for(unsigned i=0;i<80;i++){m.tick(r);if(r.operator_latched==want)return;}assert(false);};
    m.mode(10);m.words[0x532d48]=1;m.words[0x54bdf8]=5;
    for(unsigned i=0;i<25;i++)m.tick(r);
    m.words[0x532d48]=3;m.words[0x54bda4]=3;
    for(unsigned i=0;i<25;i++)m.tick(r);
    m.byte(0x54c0e9,5);pulse(8);m.words[0x54bda4]=8;pulse(4);
    m.words[0x532d48]=1;m.words[0x54bdf8]=10;
    for(unsigned i=0;i<25;i++)m.tick(r);
    m.mode(0);for(unsigned i=0;i<40;i++)m.tick(r);
    assert(r.game_options_busy&&r.game_options_status==GameOptionsStatus::Saving);
    assert(!r.take_game_options_reset_request());
    if(corrupt_calibration)m.byte(0x54c0f1,99);
    m.words[0x54c0dc]=1;m.tick(r);
    if(corrupt_calibration){assert(r.game_options_status==GameOptionsStatus::Failed);assert(!r.take_game_options_reset_request());return;}
    assert(r.game_options_busy&&r.game_options_status==GameOptionsStatus::Restarting);
    assert(!r.operator_latched&&r.take_game_options_reset_request());
    for(unsigned i=0;i<30;i++)m.tick(r);
    assert(r.game_options_busy&&r.game_options_status==GameOptionsStatus::Restarting);
    m.mode(11);m.tick(r);m.mode(0);if(mismatch)m.byte(0x54c0e9,3);m.tick(r);
    assert(!r.game_options_busy&&r.engine_pause_requested());
    assert(r.game_options_status==(mismatch?GameOptionsStatus::Failed:GameOptionsStatus::Complete));
    if(mismatch)assert(r.game_options_error==GameOptionsError::NativeMismatch);
    assert(!r.take_game_options_reset_request()&&!r.operator_latched);
}
int main(){active_round_requires_real_boot_before_service();manual_service_and_untrusted_requests_never_reset();focus_loss_timeout_and_manual_service_cancel_pending_reset();failure_during_original_save_releases_inputs_without_final_reset();save_requires_ack_and_final_boot_readback();save_requires_ack_and_final_boot_readback(true);save_requires_ack_and_final_boot_readback(false,true);std::cout<<"game_options_restart: passed\n";}
