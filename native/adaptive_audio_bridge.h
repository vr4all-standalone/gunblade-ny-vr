// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "audio_stretch_api.h"
#include <algorithm>
#include <memory>
#include <utility>
namespace gunblade {
inline bool audio_stretch_requested(const char* vr,const char* stretch,const char* reference)noexcept{
    return vr&&stretch&&stretch[0]=='1'&&stretch[1]=='\0'&&!reference;
}
inline const char* audio_stretch_library(const char* audio,const char* xr)noexcept{
    return audio&&audio[0]?audio:xr;
}
struct AudioStretchFunctions {
    decltype(&gunblade_audio_create) create{};decltype(&gunblade_audio_push) push{};
    decltype(&gunblade_audio_read) read{};decltype(&gunblade_audio_pause) pause{};
    decltype(&gunblade_audio_stats) stats{};decltype(&gunblade_audio_destroy) destroy{};
};
enum class AudioStretchOpen {Ready,Unsupported,MissingExports,CreateFailed,InvalidStats};
class AudioStretchBridge {
public:
    AudioStretchBridge()=default;
    AudioStretchBridge(const AudioStretchBridge&)=delete;
    AudioStretchBridge& operator=(const AudioStretchBridge&)=delete;
    ~AudioStretchBridge(){reset();}
    AudioStretchOpen open(AudioStretchFunctions api,std::shared_ptr<void> module,uint32_t rate,uint32_t channels) noexcept {
        reset();
        if(rate!=48000||channels!=2)return AudioStretchOpen::Unsupported;
        if(!module||!api.create||!api.push||!api.read||!api.pause||!api.stats||!api.destroy)return AudioStretchOpen::MissingExports;
        api_=api;module_=std::move(module);handle_=api_.create(rate,channels);
        if(!handle_){reset();return AudioStretchOpen::CreateFailed;}
        GunbladeAudioStats initial{};
        if(!snapshot(initial)||initial.state>GunbladeAudioPaused){reset();return AudioStretchOpen::InvalidStats;}
        return AudioStretchOpen::Ready;
    }
    bool active()const noexcept{return handle_!=nullptr;}
    // The caller first closes SDL, preventing further consumer callbacks. The
    // worker is joined by destroy before this bridge releases its own module.
    void reset() noexcept {
        if(handle_)api_.destroy(handle_);
        handle_=nullptr;api_={};module_.reset();
    }
    uint32_t push(const int16_t* data,uint32_t frames,double now) noexcept {
        return handle_?api_.push(handle_,data,frames,now):0;
    }
    // Immutable bindings and handle during SDL playback. Pause/read are C ABI
    // operations on preallocated storage; no ownership or stats work occurs here.
    uint32_t read(int16_t* data,uint32_t frames,double now,bool paused) noexcept {
        if(!handle_){std::fill_n(data,size_t(frames)*2,int16_t(0));return 0;}
        api_.pause(handle_,paused?1u:0u);
        return api_.read(handle_,data,frames,now);
    }
    bool snapshot(GunbladeAudioStats& out)const noexcept {
        return handle_&&api_.stats(handle_,&out,sizeof(out))==1&&out.size==sizeof(out)
            &&out.abi==GunbladeAudioAbi&&out.sample_rate==48000&&out.channels==2;
    }
private:
    std::shared_ptr<void> module_;
    AudioStretchFunctions api_;
    void* handle_{};
};
struct AudioStretchRecord {double seconds{};GunbladeAudioStats total,delta;};
class AudioStretchProbe {
public:
    void begin(double now,const GunbladeAudioStats& stats)noexcept{last_time_=now;last_=stats;}
    bool due(double now)const noexcept{return now-last_time_>=1.;}
    AudioStretchRecord take(const GunbladeAudioStats& stats,double now)noexcept{
        AudioStretchRecord result;result.seconds=std::max(0.,now-last_time_);result.total=stats;
        auto delta=[](uint64_t current,uint64_t before){return current>=before?current-before:current;};
#define GUNBLADE_AUDIO_DELTA(field) result.delta.field=delta(stats.field,last_.field)
        GUNBLADE_AUDIO_DELTA(native_frames);GUNBLADE_AUDIO_DELTA(output_frames);
        GUNBLADE_AUDIO_DELTA(requested_frames);GUNBLADE_AUDIO_DELTA(delivered_frames);
        GUNBLADE_AUDIO_DELTA(missing_frames);GUNBLADE_AUDIO_DELTA(warmup_frames);
        GUNBLADE_AUDIO_DELTA(paused_frames);GUNBLADE_AUDIO_DELTA(paused_input_frames);
        GUNBLADE_AUDIO_DELTA(input_dropped_frames);GUNBLADE_AUDIO_DELTA(output_dropped_frames);
        GUNBLADE_AUDIO_DELTA(callbacks);GUNBLADE_AUDIO_DELTA(underrun_callbacks);
        GUNBLADE_AUDIO_DELTA(input_blocks);GUNBLADE_AUDIO_DELTA(output_blocks);
#undef GUNBLADE_AUDIO_DELTA
        begin(now,stats);return result;
    }
private:
    double last_time_{};
    GunbladeAudioStats last_;
};
}
