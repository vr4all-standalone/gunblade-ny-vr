// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "diagnostic_capture.h"
#include "native_reticle.h"
#include "game_state.h"
#include <future>
#include <cstdlib>
namespace gunblade {
// Captures require a private diagnostic path. Normal play performs no frame I/O.
class SceneDiagnostics {
    std::filesystem::path root_;
    std::future<std::filesystem::path> job_;
    std::array<uint32_t,2> flags_{~0u,~0u};
    bool usable_{};double logged_{-100};unsigned captures_{},timed_{};uint64_t last_sequence_{};
    std::array<uint64_t,4> failed_signatures_{};unsigned failures_{};
public:
    SceneDiagnostics(){if(const char* path=std::getenv("GUNBLADE_RETICLE_DIAGNOSTICS");path&&*path)
        root_=std::filesystem::path(path)/("session-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));}
    template<class Log> void observe(const NativeReticleDiagnostics& d,const GameState& state,std::shared_ptr<const NativeFrame> frame,Log log){
        if(!frame)return;
        if(job_.valid()&&job_.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{log("Scene diagnostic saved: "+job_.get().string());}catch(const std::exception& e){log(std::string("Scene diagnostic error: ")+e.what());}}
        std::array<uint32_t,2> flags{d.players[0].reason_flags,d.players[1].reason_flags};
        if(frame->time<logged_||flags!=flags_||usable_!=d.scene_usable||frame->time-logged_>=5){
            std::ostringstream out;out<<"Scene status: sequence "<<d.sequence<<", native time "<<frame->time<<", mode "<<state.mode<<", submode "<<state.submode<<", complete "<<frame->complete<<", rejected "<<frame->rejected<<", polygons "<<frame->polygons.size()<<", usable "<<d.scene_usable<<", resources "<<d.resources_valid<<", geometry "<<d.geometry_available<<", rasters "<<frame->background.size()<<'/'<<frame->foreground.size()<<'/'<<frame->native.size();
            for(unsigned p=0;p<2;p++){const auto& q=d.players[p];out<<", P"<<p+1<<" reasons "<<q.reason_flags<<" known "<<q.known_polygons<<" selected "<<q.selected_polygons<<" suppressed "<<q.suppressed_polygons<<" visible "<<q.visible_polygons<<" windows";for(auto n:q.windows)out<<'/'<<n;}
            log(out.str());logged_=frame->time;flags_=flags;usable_=d.scene_usable;
        }
        if(root_.empty()||job_.valid()||captures_>=8||frame->sequence==last_sequence_)return;
        constexpr double at[]={90,120,160,210};bool timed=timed_<4&&frame->time>=at[timed_];
        const bool active=state.valid&&state.playing&&state.submode==2;
        uint64_t signature=uint64_t(flags[0])|(uint64_t(flags[1])<<24)|(uint64_t(d.scene_usable)<<63);
        const bool bad_player=(state.players[0].alive()&&flags[0])||(state.players[1].alive()&&flags[1]);
        bool failure=active&&frame->time>=80&&(!d.scene_usable||((d.players[0].known_polygons||d.players[1].known_polygons||d.unknown_focus600_polygons)&&bad_player));
        for(unsigned i=0;i<failures_;i++)if(failed_signatures_[i]==signature)failure=false;
        failure=failure&&failures_<failed_signatures_.size();
        if(!timed&&!failure)return;
        if(timed)++timed_;
        if(failure)failed_signatures_[failures_++]=signature;
        ++captures_;last_sequence_=frame->sequence;auto root=root_;
        log("Scene diagnostic queued: sequence "+std::to_string(frame->sequence)+", native time "+std::to_string(frame->time));
        job_=std::async(std::launch::async,[frame=std::move(frame),root]{return diagnostics::write_frame(*frame,root);});
    }
};
}
