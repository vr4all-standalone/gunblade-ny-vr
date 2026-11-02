// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "input_policy.h"
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
namespace gunblade {
inline Settings read_settings(std::istream& input,bool* migrated=nullptr) {
    bool has_custom_min=false,has_world=false;
    Settings out;std::string line;int version{};bool has_wider{},selected_wider{},has_hand{},has_reticle_distance{};int selected_hand{};float selected_angle{};
    while(std::getline(input,line)) {
        auto equals=line.find('=');if(equals==std::string::npos)continue;
        auto key=line.substr(0,equals);std::istringstream value(line.substr(equals+1));double number{};char extra{};
        if(!(value>>number)||(value>>extra)||!std::isfinite(number))continue;
        auto integer=[&](int lo,int hi){return number==std::floor(number)&&number>=lo&&number<=hi;};
        if(key=="version"&&integer(1,1000000))version=int(number);
        else if(key=="p1_hand"&&integer(0,1)){has_hand=true;selected_hand=int(number);}
        else if(key=="gun"&&integer(0,1))out.gun=int(number);
        else if(key=="laser"&&integer(0,1))out.laser=number!=0;
        else if(key=="laser_thickness"&&integer(0,3))out.laser_thickness=int(number);
        else if(key=="wider_view_fade"&&integer(0,1))out.wider_view_fade=number!=0;
        else if(key=="wider_view"&&integer(0,1)){has_wider=true;selected_wider=number!=0;}
        else if(key=="background"&&integer(0,1))out.background=BackgroundMode(int(number));
        else if(key=="view_fov"&&number>=35&&number<=95)out.view_fov=float(number);
        else if(key=="world_scale"&&number>=.01&&number<=1){out.world_scale=float(number);has_world=true;}
        else if(key=="hud_size"&&number>=.5&&number<=2)out.hud_size=float(number);
        else if(key=="panel_distance"&&number>=.5&&number<=10)out.panel_distance=float(number);
        else if(key=="gun_angle"&&number>=-30&&number<=30)selected_angle=float(number);
        else if(key=="gun_scale"&&number>=.4&&number<=1.6)out.gun_scale=float(number);
        else if(key=="reticle_min_size"&&number>=.05&&number<=1)out.reticle_min_size=float(number);
        else if(key=="reticle_max_size"&&number>=.25&&number<=3)out.reticle_max_size=float(number);
        else if(key=="reticle_custom_min_size"&&number>=.05&&number<=1){out.reticle_custom_min_size=float(number);has_custom_min=true;}
        else if(key=="reticle_custom_max_size"&&number>=.25&&number<=3)out.reticle_custom_max_size=float(number);
        else if(key=="reticle_mode"&&integer(0,2))out.reticle_mode=reticle_mode_value(int(number));
        else if(key=="reticle_restore_mode"&&integer(0,1))out.reticle_restore_mode=reticle_mode_value(int(number));
        else if(key=="reticle_shrink_distance"&&number>=.2&&number<=20){out.reticle_shrink_distance=float(number);has_reticle_distance=true;}
        else if(key=="haptics"&&number>=0&&number<=1)out.haptics=HapticLevels[haptic_level(float(number))];
    }
    // Older builds started with a narrow view. Apply the requested new default
    // once; version 2 retains subsequent explicit user choices, including Off.
    if(version>=2&&has_wider)out.wider_view=selected_wider;
    // Version 3 changes the old right-hand default once. Later explicit hand
    // reversals retain their saved value, independently of view migration.
    if(version>=3&&has_hand)out.p1_hand=selected_hand;
    // Version 4 adopts the overseer angle baseline once, retaining later edits.
    if(version>=4)out.gun_angle=selected_angle;
    // Missing new controls retain the original perspective rate for this panel.
    if(!has_reticle_distance)out.reticle_shrink_distance=out.panel_distance;
    // Numeric size is now authoritative; removed preset names must never snap
    // an existing personal size. The user requested CM Normal for this trial.
    out.world_preset=has_world?world_preset_for(out.world_scale,Settings{}.world_scale):WorldPreset::Normal;
    if(version<8){out.laser_thickness=0;out.wider_view_fade=false;}
    if(!out.wider_view)out.wider_view_fade=false;
    if(migrated)*migrated=version<9;
    if(!has_custom_min)out.reticle_custom_min_size=out.reticle_min_size;
    if(out.reticle_mode!=ReticleMode::None)out.reticle_restore_mode=out.reticle_mode;
    return out;
}
inline void write_settings(std::ostream& out,const Settings& s) {
    const auto restore=s.reticle_mode!=ReticleMode::None?s.reticle_mode:
        s.reticle_restore_mode==ReticleMode::Custom?ReticleMode::Custom:ReticleMode::Ingame;
    out<<"version=9\np1_hand="<<s.p1_hand<<"\ngun="<<s.gun<<"\nlaser="<<s.laser<<"\nlaser_thickness="<<s.laser_thickness<<"\nwider_view_fade="<<(s.wider_view&&s.wider_view_fade)
       <<"\nview_fov="<<s.view_fov<<"\nwider_view="<<s.wider_view<<"\nworld_scale="<<s.world_scale<<"\npanel_distance="<<s.panel_distance<<"\nhud_size="<<s.hud_size<<"\ngun_angle="<<s.gun_angle<<"\ngun_scale="<<s.gun_scale<<"\nreticle_mode="<<int(s.reticle_mode)<<"\nreticle_min_size="<<s.reticle_min_size<<"\nreticle_max_size="<<s.reticle_max_size<<"\nreticle_custom_min_size="<<s.reticle_custom_min_size<<"\nreticle_custom_max_size="<<s.reticle_custom_max_size<<"\nreticle_shrink_distance="<<s.reticle_shrink_distance<<"\nhaptics="<<s.haptics<<"\nbackground="<<int(s.background)<<"\nreticle_restore_mode="<<int(restore)<<'\n';
}
}
