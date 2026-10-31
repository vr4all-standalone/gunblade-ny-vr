#include "../native/gun_pose.h"
#include "../native/gun_selection.h"
#include "../native/controller_help_text.h"
#include "../native/controller_controls.h"
#include <cassert>
using namespace gunblade;
int main(){
    assert(next_gun(0,1)==1&&next_gun(1,1)==0&&next_gun(0,-1)==1);
    assert(normalize_gun(2)==0);
    auto zero=pitched_gun_pose(glm::mat4(1),0);
    auto expected=glm::rotate(glm::mat4(1),glm::radians(-10.f),glm::vec3(1,0,0));
    assert(glm::length(glm::vec4(zero[2]-expected[2]))<1e-6f);
    assert(pitched_gun_pose(glm::mat4(1),100)==pitched_gun_pose(glm::mat4(1),30));
    Settings settings;GameState state;
    for(int hand=0;hand<2;hand++){
        auto initial=controller_help::words(hand,settings,state);
        assert(initial[1].find("P1 Start")!=std::string::npos);
        assert(initial[0].empty()&&initial[3].find("P1 Hold fire")!=std::string::npos);
        assert(initial[4].find("click")!=std::string::npos);
        assert(initial[5].empty());
    }
    state.valid=true;state.players[0].state=1;
    auto solo_right=controller_help::words(1,settings,state);
    assert(solo_right[1].find("P2 Start")!=std::string::npos&&solo_right[3].find("P1 Hold fire")!=std::string::npos);
    state.players[1].state=1;
    auto dual_right=controller_help::words(1,settings,state);
    assert(dual_right[3].find("P2 Hold fire")!=std::string::npos);
    settings.p1_hand=1;
    assert(controller_help::words(1,settings,state)[3].find("P1 Hold fire")!=std::string::npos);
    auto menu=controller_help::words(1,settings,state,true);
    assert(menu[1].find("Choose")!=std::string::npos&&menu[3].find("Choose")!=std::string::npos&&menu[4].find("Navigate")!=std::string::npos);
    auto service=controller_help::words(0,settings,state,false,true);
    assert(service[0].find("Service next")!=std::string::npos&&service[1].find("Test select")!=std::string::npos);
    auto calibration=controller_help::words(1,settings,state,false,true,true);
    assert(calibration[1].find("P1 Start")!=std::string::npos&&calibration[3].find("Aim setting")!=std::string::npos);
    ShortcutPolicy shortcuts;ExtraControls extra;Hands hands;
    for(auto& hand:hands)hand.tracked=true;
    settings.world_scale=.1f;settings.gun_scale=1;
    shortcuts.update(0,extra,hands,settings,true);
    extra.grip={true,true};shortcuts.update(.1,extra,hands,settings,true);
    extra.stick[0]={0,1};
    for(int i=2;i<12;i++)shortcuts.update(i*.1,extra,hands,settings,true);
    assert(settings.world_scale==.1f&&settings.gun_scale==1);
    extra.stick[0]={1,0};
    for(int i=12;i<22;i++)shortcuts.update(i*.1,extra,hands,settings,true);
    assert(settings.world_scale==.1f&&settings.gun_scale==1);
    extra.click[0]=true;
    assert(shortcuts.update(2.3,extra,hands,settings,true).recenter);
}
