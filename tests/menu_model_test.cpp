#include "../native/menu_model.h"
#include "../native/game_state.h"
#include <cassert>
#include <algorithm>
using namespace gunblade;
int main(){
 auto gun=menu_actions(MenuPage::Gun);assert(gun.size()==6&&gun[0]==MenuAction::GunModel&&gun[5]==MenuAction::OpenGunAdvanced);
 auto world=menu_actions(MenuPage::World);assert((world==std::vector<MenuAction>{MenuAction::WiderView,MenuAction::OpenWorldAdvanced}));
 assert(std::string(menu_label(MenuAction::TabGame))=="In-game");
 for(int i=0;i<4;i++)assert(menu_tab_index(menu_tab_page(MenuTabs[i]))==i);
 assert(MenuTabs.front()==MenuAction::TabGun&&MenuTabs.back()==MenuAction::TabGame);
 GameState phase;phase.valid=true;phase.mode=11;assert(!menu_pauses_native(true,true,phase));
 assert(menu_pauses_native(true,false,phase));phase.ready=true;assert(menu_pauses_native(true,true,phase));
 phase.ready=false;phase.playing=true;assert(menu_pauses_native(true,true,phase));
 phase.playing=false;phase.service=true;assert(menu_pauses_native(true,true,phase));
 assert(!menu_pauses_native(false,true,phase));
 auto adv=menu_actions(MenuPage::WorldAdvanced);assert((adv==std::vector<MenuAction>{MenuAction::WorldScale,MenuAction::HudSize,MenuAction::PanelDistance,MenuAction::ViewWidth}));
 auto system=menu_actions(MenuPage::System);assert(system[0]==MenuAction::TwoPlayers&&system[1]==MenuAction::OpenService&&system[3]==MenuAction::ResetDefaults&&system[4]==MenuAction::Quit);
 auto advanced=menu_actions(MenuPage::GunAdvanced);assert(advanced.front()==MenuAction::GunSize&&advanced.size()==4);
 std::vector<MenuAction> native{MenuAction::GameDifficulty,MenuAction::GameShifting,MenuAction::GameLives,MenuAction::GameAttractSound};assert(menu_actions(MenuPage::Game,native)==native);
 assert(main_page(MenuPage::GunAdvanced)==MenuPage::Gun&&main_page(MenuPage::WorldAdvanced)==MenuPage::World);
 assert(std::string(menu_label(MenuAction::GunModel))=="Gun model");
 for(auto page:{MenuPage::Gun,MenuPage::GunAdvanced,MenuPage::World,MenuPage::WorldAdvanced,MenuPage::System})for(auto action:menu_actions(page))assert(std::string(menu_label(action))!="Resume"&&std::string(menu_label(action))!="Recenter"&&std::string(menu_label(action))!="Fire mode");
}
