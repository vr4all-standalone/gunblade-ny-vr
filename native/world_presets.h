// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
namespace gunblade {
enum class WorldPreset {Custom,Normal,Small,Large};
inline bool valid_world_scale(float n){return std::isfinite(n)&&n>=.01f&&n<=1.f;}
inline float world_preset_scale(WorldPreset preset,float normal,float custom){
 if(!valid_world_scale(normal))normal=.08f;
 switch(preset){case WorldPreset::Small:return std::clamp(normal*.75f,.01f,1.f);case WorldPreset::Large:return std::clamp(normal*1.25f,.01f,1.f);case WorldPreset::Normal:return normal;default:return valid_world_scale(custom)?custom:normal;}
}
inline WorldPreset world_preset_for(float scale,float normal){
 for(auto p:{WorldPreset::Normal,WorldPreset::Small,WorldPreset::Large})if(std::abs(scale-world_preset_scale(p,normal,normal))<.000001f)return p;
 return WorldPreset::Custom;
}
template<class Settings> inline void apply_world_preset(Settings& s,WorldPreset preset,float normal){s.world_preset=preset;s.world_scale=world_preset_scale(preset,normal,s.world_scale);}
template<class Settings> inline void set_custom_world_scale(Settings& s,float value,float normal){s.world_scale=valid_world_scale(value)?value:normal;s.world_preset=world_preset_for(s.world_scale,normal);}
// Normal exactly matches Confidential Mission's 0.8 mm half-width glow beam.
// The live menu previews the same choice, independently of gameplay Laser Off.
inline float laser_half_width(int choice,bool){return choice==1?.0016f:choice==2?.004f:choice==3?.008f:.0008f;}
}
