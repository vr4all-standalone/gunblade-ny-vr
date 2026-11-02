// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "input_policy.h"
#include "game_state.h"
#include <string>
namespace gunblade::controller_help {
using Words=std::array<std::string,6>;
inline Words words(int hand,const Settings&,const GameState&,bool menu=false,bool service=false,bool service_guns=false) {
    const std::string start=hand==0?"X":"A",aux=hand==0?"Y":"B";
    if(menu)return {"",start+" - Choose","Menu - Return","Trigger - Choose","Stick - Navigate",""};
    if(service){
        if(service_guns)return {"",start+" - Start","Menu","Trigger - Aim setting",hand==1?"Stick click - Menu controls":"",""};
        return {aux+" - Service next",start+" - Test select","Menu","Trigger - Test select","Stick down - Service next",""};
    }
    return {"",start+" - Start","Menu",
        "Trigger - Fire",
        hand==0?"Stick click - Recenter":"Click - Laser/crosshair",""};
}
}
