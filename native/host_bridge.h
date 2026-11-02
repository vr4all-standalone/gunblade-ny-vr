// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "shared_runtime.h"
#include "modules/lib/osdlib.h"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#ifdef __ANDROID__
#include <jni.h>
extern "C" void* SDL_AndroidGetJNIEnv(void);
extern "C" void* SDL_AndroidGetActivity(void);
#endif
namespace gunblade {
inline bool vr_requested(){static const bool enabled=std::getenv("GUNBLADE_VR")!=nullptr;return enabled;}
class HostBridge {
    osd::dynamic_module::ptr module_;void(*stop_)(){};
public:
    HostBridge() {
        if(!vr_requested())return;
        const char* name=std::getenv("GUNBLADE_XR_LIBRARY");
#ifdef _WIN32
        module_=osd::dynamic_module::open({name?name:"gunblade_xr.dll"});
#else
        module_=osd::dynamic_module::open({name?name:"libgunblade_xr.so"});
#endif
        auto start=module_->bind<bool(*)(SharedRuntime*)>("gunblade_xr_start");
        stop_=module_->bind<void(*)()>("gunblade_xr_stop");
#ifdef __ANDROID__
        auto init=module_->bind<void(*)(void*,void*)>("gunblade_xr_android_init");
        auto env=static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());JavaVM* vm{};
        if(env&&init){env->GetJavaVM(&vm);auto activity=SDL_AndroidGetActivity();init(vm,activity);env->DeleteLocalRef(static_cast<jobject>(activity));}
#endif
        if(!start||!stop_||!start(&shared_runtime())){std::cerr<<"Gunblade NY VR: host could not start; check the library and log.\n";shared_runtime().quit_requested=true;}
    }
    ~HostBridge(){if(stop_)stop_();}
};
inline std::unique_ptr<HostBridge>& host_bridge_slot(){static std::unique_ptr<HostBridge> bridge;return bridge;}
inline void start_host(){auto& slot=host_bridge_slot();if(!slot){shared_runtime().reset();slot=std::make_unique<HostBridge>();}}
inline void stop_host(){host_bridge_slot().reset();shared_runtime().reset();}
template<class Machine> void poll_host(Machine& machine) {
    if(!vr_requested())return;
    start_host();auto& runtime=shared_runtime();
    if(runtime.quit_requested)machine.schedule_exit();
    else if(runtime.take_game_options_reset_request())machine.schedule_soft_reset();
    if(runtime.host_ready){const bool pause=runtime.engine_pause_requested();if(pause&&!machine.paused()){runtime.release_inputs();machine.pause();}else if(!pause&&machine.paused())machine.resume();}
    using Clock=std::chrono::steady_clock;static auto last=Clock::now();static double native_before=machine.time().as_double();
    auto now=Clock::now();static auto previous_poll=now;static bool previous_paused=machine.paused();static double paused_wall{};
    if(previous_paused)paused_wall+=std::chrono::duration<double>(now-previous_poll).count();
    previous_poll=now;previous_paused=machine.paused();
    double elapsed=std::chrono::duration<double>(now-last).count();
    if(elapsed>=5){double native=machine.time().as_double();auto frame=runtime.scene.load();
        if(const char* path=std::getenv("GUNBLADE_XR_LOG")){
            const double active=std::max(0.,elapsed-paused_wall);
            std::ofstream out(std::string(path)+".engine",std::ios::app);
            out<<"Native seconds "<<native<<", speed "<<(native-native_before)/elapsed*100
               <<"%, active speed "<<(active>.001?(native-native_before)/active*100:0)
               <<"%, active wall "<<active<<", captured frame "<<(frame?frame->sequence:0)<<", paused "<<machine.paused()<<'\n';
        }last=now;native_before=native;paused_wall=0;
    }
}
inline Cabinet cabinet_input(){auto& runtime=shared_runtime();std::lock_guard lock(runtime.mutex);return runtime.engine_input.cabinet;}
inline uint8_t system_input(uint8_t native) {
    if(!vr_requested())return native;
    auto input=cabinet_input();native&=~uint8_t(shared_runtime().operator_latched.load()&0x3c);
    using Clock=std::chrono::steady_clock;static Clock::time_point until{};
    if(shared_runtime().service_requested.exchange(false))until=Clock::now()+std::chrono::milliseconds(200);
    input.test=input.test||Clock::now()<until;
    if(shared_runtime().game_options_busy){input={};until={};}
    return in0(native,input);
}
inline uint8_t trigger_input(uint8_t native){return vr_requested()?in1(native,cabinet_input()):native;}
inline uint8_t analog_input(unsigned channel,uint8_t native){return vr_requested()?adc(channel,cabinet_input()):native;}
template<class Read32> void native_tick(Read32 read) {
    if(!vr_requested())return;
    auto& runtime=shared_runtime();std::lock_guard lock(runtime.mutex);
    auto state=read_game_state(read);
    runtime.tick_game_options(read,state);
    const bool had_players=runtime.state.players[0].joined()||runtime.state.players[1].joined();
    const bool has_players=state.players[0].joined()||state.players[1].joined();
    if((had_players&&!has_players&&!state.service)||(state.mode==11&&runtime.state.mode!=11))++runtime.session;
    state.session=runtime.session;
    runtime.shot_tracker.update(state,read);
    // The accepted native frame used the cabinet owner latched on the previous
    // tick. Record that owner before accepting the next desired XR input.
    runtime.shot_ownership.update(state,runtime.state,runtime.engine_input);
    for(unsigned p=0;p<2;p++)runtime.calibration[p]=read_native_calibration(read,p);
    runtime.state=state;
    runtime.engine_input=runtime.game_options_busy?CabinetInput{}:runtime.input_latch.update(runtime.input);

}
}
