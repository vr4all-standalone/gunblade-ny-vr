// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "host_bridge.h"
namespace gunblade {
inline SceneCapture& live_capture(){static SceneCapture capture;return capture;}
inline bool& live_active(){static bool active{};return active;}
inline bool& live_pending(){static bool pending{};return pending;}
inline std::unique_ptr<NativeFrame>& live_raster(){static std::unique_ptr<NativeFrame> frame;return frame;}
inline void live_begin(uint64_t sequence,double time) {
    live_active()=vr_requested();
    if(live_active()){live_capture().begin(sequence,time);live_pending()=false;}
}
template<class Pixel> void copy_pixels(std::vector<uint32_t>& out,int width,int height,Pixel pixel) {
    out.resize(size_t(width)*height);
    for(int y=0;y<height;y++)for(int x=0;x<width;x++)out[size_t(y)*width+x]=uint32_t(pixel(x,y));
}
}
