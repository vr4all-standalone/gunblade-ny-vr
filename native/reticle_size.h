// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "input_policy.h"
#include <algorithm>
#include <cmath>
namespace gunblade {
struct ReticleSize {float minimum_ratio{.22f},shrink_distance{6.7f},maximum_ratio{1.f};};
inline ReticleSize reticle_size_settings(float minimum_ratio,float shrink_distance,float maximum_ratio=1.f){
    return {std::isfinite(minimum_ratio)?std::clamp(minimum_ratio,.05f,1.f):.22f,
        std::isfinite(shrink_distance)?std::clamp(shrink_distance,.2f,20.f):6.7f,
        std::isfinite(maximum_ratio)?std::clamp(maximum_ratio,.25f,3.f):1.f};
}
inline ReticleSize reticle_size_for_mode(const Settings& s){
    const bool custom=s.reticle_mode==ReticleMode::Custom;
    return reticle_size_settings(custom?s.reticle_custom_min_size:s.reticle_min_size,
        s.reticle_shrink_distance,custom?s.reticle_custom_max_size:s.reticle_max_size);
}
// Returns the physical reference used only for original native polygon offsets.
// The marker's target position and depth are independent of this size policy.
inline double reticle_size_reference(double target_distance,double panel_distance,ReticleSize settings={}){
    if(!std::isfinite(panel_distance)||panel_distance<=0)return 0;
    settings=reticle_size_settings(settings.minimum_ratio,settings.shrink_distance,settings.maximum_ratio);
    if(!std::isfinite(target_distance)||target_distance<=panel_distance)return panel_distance*settings.maximum_ratio;
    const double perceived=std::max(double(settings.minimum_ratio),1./(1.+(target_distance-panel_distance)/settings.shrink_distance));
    return target_distance*perceived*settings.maximum_ratio;
}
}
