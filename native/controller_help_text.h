// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "input_policy.h"
#include "game_state.h"
#include <string>
namespace gunblade::controller_help {
using Words=std::array<std::string,6>;
inline Words words(int hand,const Settings& settings,const GameState& state,bool menu=false,bool service=false,bool service_guns=false) {
    const std::string start=hand==0?"X":"A",aux=hand==0?"Y":"B";
    if(menu)return {"",start+" - Choose","Menu - Return","Trigger - Choose","Stick - Navigate",""};
    const bool p1=state.valid&&state.players[0].joined(),p2=state.valid&&state.players[1].joined();
    const int assigned=hand==settings.p1_hand?0:1;
    const int start_player=(!p1&&!p2)?0:assigned;
    const int gun_player=p1&&p2?assigned:p2?1:0;
    if(service){
        if(service_guns)return {"",start+" - P"+std::to_string(assigned+1)+" Start","Menu","Trigger - Aim setting",hand==1?"Stick click - Menu controls":"",""};
        return {aux+" - Service next",start+" - Test select","Menu","Trigger - Test select","Stick down - Service next",""};
    }
    return {"",start+" - P"+std::to_string(start_player+1)+" Start","Menu",
        "Trigger - P"+std::to_string(gun_player+1)+" Hold fire",
        hand==0?"Stick click - Recenter":"Stick click - Laser",""};
}
}
