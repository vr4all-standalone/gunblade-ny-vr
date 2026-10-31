// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model2_scene.h"
#include "game_state.h"
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <string_view>
namespace gunblade {

// Only the producer's unpublished frame is mutable here. Direct framebuffer
// mode must publish the native image even when the DSP produces no new geometry.
// Retain the original count for diagnostics; the independent trace is untouched.
inline size_t prepare_direct_framebuffer_frame(std::unique_ptr<NativeFrame>& frame,bool direct,
                                               uint64_t sequence,double time,int xoffset,int yoffset,
                                               int width,int height) {
    const size_t captured=frame?frame->polygons.size():0;
    if(!direct)return captured;
    if(!frame) {
        frame=std::make_unique<NativeFrame>();frame->sequence=sequence;frame->time=time;
        frame->xoffset=xoffset;frame->yoffset=yoffset;frame->width=width;frame->height=height;
        frame->complete=false;
    }
    frame->polygons.clear();return captured;
}
enum class RasterDecision { CpuOriginal,DirectFramebuffer,Disabled,Reference,HostUnavailable,NotPlaying,MissingFrame,InvalidFrame,MissingResources,NoVisibleWorld,Skip };
struct RasterContext {
    bool vr{},requested{},cpu_reference{},capture{},host_ready{},quit{},full_frame{true},direct_framebuffer{};
    GameState state;
    size_t captured_polygons{};
};
inline bool observed_area2_world_polygon(const NativeFrame& frame,const NativePolygon& polygon) {
    // Live Revision A Area2 frame6902: focus407 world geometry is in window2
    // while every window1 triangle is offscreen. Native window numbers count
    // commands. Accept only this observed full-display world profile here;
    // focus600 reticles/HUD and unverified camera profiles retain fallback.
    if(frame.width!=496||frame.height!=384||frame.xoffset!=0||frame.yoffset!=128||
       polygon.focus!=std::array<float,2>{407,407}||polygon.center!=std::array<float,2>{248,320}||
       polygon.count<3||polygon.count>4||polygon.viewport[0]>0||polygon.viewport[2]<495||
       int64_t(384)-polygon.viewport[3]+128>0||int64_t(384)-polygon.viewport[1]+128<383)return false;
    // Positive perspective depth variation excludes a planar screen overlay;
    // the existing exact clipping still has to find a visible nonzero triangle.
    for(unsigned i=1;i<polygon.count;++i)
        if(std::isfinite(polygon.vertices[i].z)&&std::isfinite(polygon.vertices[0].z)&&
           std::abs(polygon.vertices[i].z-polygon.vertices[0].z)>.0001f)return true;
    return false;
}
inline RasterDecision raster_decision(const RasterContext& context,const NativeFrame* frame) {
    if(context.direct_framebuffer)return RasterDecision::DirectFramebuffer;
    if(!context.vr || !context.requested)return RasterDecision::Disabled;
    if(context.cpu_reference || context.capture)return RasterDecision::Reference;
    if(!context.host_ready || context.quit)return RasterDecision::HostUnavailable;
    if(!context.state.valid || !context.state.playing || context.state.mode!=2 ||
       context.state.submode!=2 || context.state.service)return RasterDecision::NotPlaying;
    if(!frame)return RasterDecision::MissingFrame;
    if(!context.full_frame || frame->rejected || frame->width<2 || frame->height<2 ||
       frame->width>2048 || frame->height>2048 ||
       frame->background.size()!=size_t(frame->width)*frame->height)return RasterDecision::InvalidFrame;
    if(!Model2Resources(*frame).valid())return RasterDecision::MissingResources;
    // Existence does not depend on native draw order. Reuse the host's exact
    // clipping on one candidate at a time and stop at the first visible world
    // triangle, rather than sorting/clipping the entire scene a second time.
    // This thin frame owns no image/resource copies; fallback checks above and
    // the incomplete-frame publication rules remain unchanged.
    NativeFrame candidate;
    candidate.width=frame->width;candidate.height=frame->height;
    candidate.xoffset=frame->xoffset;candidate.yoffset=frame->yoffset;candidate.lens=frame->lens;
    for(const auto& polygon:frame->polygons) {
        if(panel_polygon(polygon)||(polygon.window!=1&&!observed_area2_world_polygon(*frame,polygon)))continue;
        candidate.polygons.assign(1,polygon);
        for(const auto& triangle:scene_triangles(candidate,false)) {
            const auto& a=triangle.vertices[0];const auto& b=triangle.vertices[1];const auto& c=triangle.vertices[2];
            const double area=(b.pixel_x-a.pixel_x)*(c.pixel_y-a.pixel_y)-(b.pixel_y-a.pixel_y)*(c.pixel_x-a.pixel_x);
            if(std::isfinite(area) && std::abs(area)>1e-5)return RasterDecision::Skip;
        }
    }
    return RasterDecision::NoVisibleWorld;
}
inline bool fast_raster_requested() {
    const char* value=std::getenv("GUNBLADE_FAST_RASTER");return value && std::string_view(value)=="1";
}
inline const char* raster_decision_name(RasterDecision decision) {
    switch(decision) {
    case RasterDecision::DirectFramebuffer:return "direct_framebuffer";
    case RasterDecision::Disabled:return "disabled";
    case RasterDecision::Reference:return "reference";
    case RasterDecision::HostUnavailable:return "host_unavailable";
    case RasterDecision::NotPlaying:return "not_playing";
    case RasterDecision::MissingFrame:return "missing_frame";
    case RasterDecision::InvalidFrame:return "invalid_frame";
    case RasterDecision::MissingResources:return "missing_resources";
    case RasterDecision::NoVisibleWorld:return "no_visible_world";
    case RasterDecision::Skip:return "skip";
    default:return "cpu_original";
    }
}
inline void record_raster_decision(uint64_t sequence,double time,const RasterContext& context,
                                   const NativeFrame* frame,RasterDecision decision) {
    static std::ofstream log;
    static bool opened=false;
    if(!opened) {
        opened=true;const char* path=std::getenv("GUNBLADE_RASTER_LOG");if(!path)return;
        log.open(path,std::ios::trunc);
        if(log)log<<"sequence\ttime\tdecision\tmode\tsubmode\tpolygons\trejected\tcaptured_polygons\n";
    }
    if(log)log<<sequence<<'\t'<<std::setprecision(17)<<time<<'\t'<<raster_decision_name(decision)
        <<'\t'<<context.state.mode<<'\t'<<context.state.submode<<'\t'
        <<(frame?frame->polygons.size():0)<<'\t'<<(frame?frame->rejected:0)<<'\t'<<context.captured_polygons<<'\n';
}
}
