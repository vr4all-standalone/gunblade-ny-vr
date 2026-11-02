#include "../native/presentation.h"
#include "../native/gl_context.h"
#include "../native/settings_io.h"
#include "../native/shared/xr_panel.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cassert>
#include <fstream>
#include <filesystem>
using namespace gunblade;
int main(int argc,char** argv){
 assert(argc==2);_putenv_s("GUNBLADE_ASSETS",argv[1]);_putenv_s("GUNBLADE_SETTINGS","menu-runtime-test.ini");GLContext gl;
 Presentation ui;Settings settings;SharedRuntime runtime;PresentationInput in;in.hands[0].tracked=in.hands[1].tracked=true;double now{};
 auto tick=[&](){now+=1;ui.update(in,settings,runtime,now);};
 auto stick=[&](glm::vec2 s){in.sticks[0]=s;tick();in.sticks[0]={};tick();};
 auto select=[&](){in.hands[0].start=true;tick();in.hands[0].start=false;tick();};
 auto seek=[&](MenuAction action){for(int n=0;n<32&&ui.selected_action()!=action;n++)stick({0,-1});assert(ui.selected_action()==action);};
 tick();in.toggle=true;tick();in.toggle=false;tick();assert(ui.menu_page()==MenuPage::Gun&&ui.menu_pointer_hand()==0);
 seek(MenuAction::TabGun);select();assert(ui.menu_page()==MenuPage::Gun);
 seek(MenuAction::Laser);stick({1,0});assert(settings.laser&&settings.laser_thickness==1);
 stick({1,0});assert(settings.laser&&settings.laser_thickness==2);
 stick({1,0});assert(settings.laser&&settings.laser_thickness==3);
 stick({1,0});assert(settings.laser_thickness==3); // Upper boundary.
 stick({-1,0});stick({-1,0});stick({-1,0});assert(settings.laser&&settings.laser_thickness==0);
 stick({-1,0});assert(!settings.laser&&ui.menu_pointer_hand()==0);
 stick({-1,0});assert(!settings.laser); // Lower boundary.
 stick({1,0});assert(settings.laser&&settings.laser_thickness==0);
 stick({1,0});assert(settings.laser&&settings.laser_thickness==1);
 seek(MenuAction::Reticle);stick({1,0});assert(settings.reticle_mode==ReticleMode::Custom);
 seek(MenuAction::OpenGunAdvanced);select();assert(ui.menu_page()==MenuPage::GunAdvanced);
 seek(MenuAction::ReticleMinimum);stick({1,0});assert(settings.reticle_custom_min_size==.23f&&settings.reticle_min_size==.22f);
 seek(MenuAction::ReticleMaximum);stick({1,0});assert(settings.reticle_custom_max_size==1.05f);
 seek(MenuAction::ReticleDistance);stick({1,0});assert(settings.reticle_shrink_distance==6.8f);
 seek(MenuAction::TabWorld);select();assert(ui.menu_page()==MenuPage::World);
 seek(MenuAction::WiderView);stick({1,0});assert(settings.wider_view&&settings.wider_view_fade);
 stick({-1,0});assert(settings.wider_view&&!settings.wider_view_fade);
 stick({-1,0});assert(!settings.wider_view&&!settings.wider_view_fade);
 stick({1,0});assert(settings.wider_view&&!settings.wider_view_fade);
 stick({1,0});assert(settings.wider_view&&settings.wider_view_fade);
 seek(MenuAction::OpenWorldAdvanced);select();assert(ui.menu_page()==MenuPage::WorldAdvanced);
 seek(MenuAction::WorldScale);stick({1,0});assert(std::abs(settings.world_scale-.085f)<.00001f);
 seek(MenuAction::HudSize);stick({1,0});assert(settings.hud_size==1.05f);
 std::ifstream saved("menu-runtime-test.ini");auto cold=read_settings(saved);assert(cold.laser_thickness==1&&cold.wider_view&&cold.wider_view_fade&&cold.hud_size==1.05f&&std::abs(cold.world_scale-.085f)<.00001f);saved.close();
 // A fresh non-owner trigger claims the only pointer without activating.
 auto prior=ui.selected_action();in.hands[1].trigger=true;tick();assert(ui.menu_pointer_hand()==1&&ui.selected_action()==prior);
 in.sticks[0]={1,0};tick();assert(settings.hud_size==1.05f);in.sticks[0]={};tick();in.hands[1].trigger=false;tick();
 in.hands[0].trigger=true;tick();assert(ui.menu_pointer_hand()==0);in.hands[0].trigger=false;tick();
 seek(MenuAction::TabSystem);select();seek(MenuAction::ResetDefaults);select();assert(settings.hud_size==1.05f); // Confirmation only.
 seek(MenuAction::Cancel);select();assert(settings.laser_thickness==1);
 seek(MenuAction::ResetDefaults);select();seek(MenuAction::Confirm);select();assert(settings.hud_size==1&&settings.laser_thickness==0&&settings.world_preset==WorldPreset::Normal&&settings.world_scale==.08f);
 runtime.game_options={true,true,3,3,3,1};runtime.state.valid=runtime.state.ready=true;
 seek(MenuAction::TabGame);select();assert(ui.menu_page()==MenuPage::Game);
 seek(MenuAction::GameDifficulty);stick({1,0});assert(runtime.game_options.difficulty==3&&!runtime.game_options_busy);
 seek(MenuAction::GameLives);stick({1,0});assert(runtime.game_options.player_life==3);
 seek(MenuAction::ApplyGameSettings);select();assert(runtime.game_options_busy&&runtime.game_options_status==GameOptionsStatus::Restarting);
 assert(runtime.game_options_reset_pending&&runtime.game_options_restart_desired.difficulty==4&&runtime.game_options_restart_desired.player_life==4);
 seek(MenuAction::Close);select();assert(ui.open()); // Original native save owns the transition.
 InputPolicy after_restart;for(auto& hand:in.hands)hand.aim={.5,.5,true,true};
 after_restart.update(now,in.hands,runtime.state,settings,true,true);
 in.hands[0].start=in.hands[0].trigger=true;
 runtime.game_options_busy=false;runtime.game_options_status=GameOptionsStatus::Complete;runtime.game_options={true,true,4,3,4,1};tick();
 assert(!ui.open()); // Successful restart acknowledgement closes the overlay.
 auto held=after_restart.update(now,in.hands,runtime.state,settings,true,ui.open());
 assert(held.cabinet.coins==0&&held.cabinet.starts==0&&!held.cabinet.trigger[0]&&!held.cabinet.trigger[1]);
 tick();held=after_restart.update(now,in.hands,runtime.state,settings,true,ui.open());assert(!held.cabinet.coins&&!held.cabinet.starts);
 in.hands[0].start=in.hands[0].trigger=false;tick();after_restart.update(now,in.hands,runtime.state,settings,true,false);
 in.hands[0].start=true;tick();assert(after_restart.update(now,in.hands,runtime.state,settings,true,false).cabinet.coins==1);
 in.hands[0].start=false;
 // Real trigger rays at row bodies/current values must edit, and staging must
 // survive closing the menu during a round without writing native settings.
 for(bool eligible_during_edit:{true,false}){
  Presentation panel;Settings prefs;SharedRuntime game;game.game_options={true,eligible_during_edit,3,3,3,1};game.state.valid=true;game.state.ready=eligible_during_edit;game.state.playing=!eligible_during_edit;
  PresentationInput hands;hands.hands[0].tracked=true;double time{};
  auto update=[&]{panel.update(hands,prefs,game,++time);};
  auto move=[&](glm::vec2 v){hands.sticks[0]=v;update();hands.sticks[0]={};update();};
  auto find=[&](MenuAction action){for(int i=0;i<40&&panel.selected_action()!=action;i++)move({0,-1});assert(panel.selected_action()==action);};
  auto press=[&]{hands.hands[0].start=true;update();hands.hands[0].start=false;update();};
  update();hands.toggle=true;update();hands.toggle=false;update();find(MenuAction::TabGame);press();
  vr::xr::SettingsPanel shape;shape.rows.resize(5);const auto layout=vr::xr::settingsLayout();
  for(int row=0;row<4;row++){
   auto rect=vr::xr::settingsRowRect(shape,row);glm::vec2 uv{.55f,rect.y+rect.h*.5f};
   glm::vec3 target{(uv.x-.5f)*layout.width,-.1f+(.5f-uv.y)*layout.height,-1.2f},from{-.25f,-.2f,-.35f};
   auto aim=glm::inverse(glm::lookAt(from,target,glm::vec3(0,1,0)));auto local=panel.muzzle(glm::mat4(1),prefs);
   aim[3]=glm::vec4(from-glm::vec3(aim*glm::vec4(local,0)),1);
   hands.poses[0]=aim*glm::rotate(glm::mat4(1),glm::radians(10.f),glm::vec3(1,0,0));update();
   hands.hands[0].trigger=true;update();hands.hands[0].trigger=false;update();
  }
  assert(!game.game_options_busy&&game.game_options.difficulty==3&&game.game_options.player_life==3);
  find(MenuAction::Close);press();assert(!panel.open());
  ++game.session;game.state.session=game.session; // Native round end changes the session, not pending UI choices.
  hands.toggle=true;update();hands.toggle=false;update();
  find(MenuAction::ApplyGameSettings);press();assert(game.game_options_busy&&game.game_options_status==GameOptionsStatus::Restarting);
  assert(game.game_options_restart_desired.difficulty==4&&game.game_options_restart_desired.shifting_difficulty==4&&game.game_options_restart_desired.player_life==4&&game.game_options_restart_desired.attract_sound==0);
  game.game_options_busy=false;game.game_options_reset_pending=false;game.game_options_restart_phase=SharedRuntime::RestartPhase::None;game.game_options_status=GameOptionsStatus::Failed;update();
  find(MenuAction::ApplyGameSettings);press();assert(game.game_options_busy&&game.game_options_restart_desired.difficulty==4);
  // A save can be interrupted after RAM changes but before EEPROM commits.
  // Do not fake a successful retry; original Service remains available.
  game.game_options_busy=false;game.game_options_reset_pending=false;game.game_options_restart_phase=SharedRuntime::RestartPhase::None;game.game_options_status=GameOptionsStatus::Failed;
  game.game_options={true,false,4,4,4,0};game.state.valid=true;game.state.service=true;update();
  find(MenuAction::ApplyGameSettings);press();assert(!game.game_options_busy);
  find(MenuAction::TabSystem);press();find(MenuAction::OpenService);press();assert(panel.service()&&!panel.open());
 }
 std::filesystem::remove("menu-runtime-test.ini");_putenv_s("GUNBLADE_SETTINGS","");
}
