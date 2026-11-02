#include "../native/controller_controls.h"
#include "../native/settings_io.h"
#include "../native/menu_model.h"
#include "../native/controller_help_text.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
using namespace gunblade;
int main(int argc,char** argv){
 assert(argc==2);const std::filesystem::path path=argv[1];
 for(bool beam:{false,true})for(auto mode:{ReticleMode::None,ReticleMode::Ingame,ReticleMode::Custom}){
  Settings s;s.laser=beam;s.reticle_mode=mode;s.laser_thickness=3;
  s.reticle_min_size=.37f;s.reticle_max_size=1.45f;s.reticle_custom_min_size=.41f;s.reticle_custom_max_size=1.65f;
  s.reticle_shrink_distance=9.3f;s.world_scale=.123f;s.p1_hand=1;
  Hands hands;for(auto& h:hands)h.tracked=true;
  ExtraControls controls;ShortcutPolicy policy;unsigned saves=0;
  auto tick=[&](bool enabled=true){auto action=policy.update(0,controls,hands,s,enabled);if(action.laser){std::ofstream f(path);write_settings(f,s);++saves;}return action;};
  tick();controls.click[1]=true;assert(tick().laser);
  const bool visible=beam||mode!=ReticleMode::None;
  assert(s.laser==!visible);
  assert(s.reticle_mode==(visible?ReticleMode::None:ReticleMode::Ingame));
  for(int i=0;i<40;i++)assert(!tick().laser);
  assert(saves==1);
  {std::ifstream f(path);s=read_settings(f);}
  assert(s.laser==!visible&&s.reticle_mode==(visible?ReticleMode::None:ReticleMode::Ingame));
  assert(s.laser_thickness==3&&s.reticle_min_size==.37f&&s.reticle_max_size==1.45f);
  assert(s.reticle_custom_min_size==.41f&&s.reticle_custom_max_size==1.65f&&s.reticle_shrink_distance==9.3f&&s.world_scale==.123f&&s.p1_hand==1);
  controls={};ShortcutPolicy cold;cold.update(1,controls,hands,s,true);controls.click[1]=true;
  assert(cold.update(1.1,controls,hands,s,true).laser);
  assert(s.laser==visible);
  assert(s.reticle_mode==(!visible?ReticleMode::None:mode==ReticleMode::None?ReticleMode::Ingame:mode));
  auto before=s;assert(!cold.update(1.2,controls,hands,s,true).laser);
  cold.update(2,controls,hands,s,false);assert(!cold.update(2.1,controls,hands,s,true).laser);
  assert(s.laser==before.laser&&s.reticle_mode==before.reticle_mode);
  controls.click[1]=false;cold.update(2.2,controls,hands,s,true);controls.click[0]=true;
  assert(cold.update(2.3,controls,hands,s,true).recenter&&s.laser==before.laser&&s.reticle_mode==before.reticle_mode);
 }
 assert(std::string(menu_label(MenuAction::Reticle))=="Crosshair");
 assert(std::string(menu_label(MenuAction::ReticleMinimum))=="Crosshair min size");
 assert(std::string(menu_label(MenuAction::ReticleMaximum))=="Crosshair max size");
 assert(std::string(menu_label(MenuAction::ReticleDistance))=="Crosshair shrink distance");
 assert(controller_help::words(1,Settings{},GameState{})[4].find("crosshair")!=std::string::npos);
 {Settings s;set_crosshair_mode(s,ReticleMode::Custom);set_crosshair_mode(s,ReticleMode::None);s.laser=false;
  std::ostringstream f;write_settings(f,s);std::istringstream in(f.str());s=read_settings(in);
  Hands hands;hands[1].tracked=true;ExtraControls controls;ShortcutPolicy policy;
  policy.update(0,controls,hands,s,true);controls.click[1]=true;policy.update(.1,controls,hands,s,true);
  assert(s.laser&&s.reticle_mode==ReticleMode::Custom);
 }
 for(const char* invalid:{"-1","2","99","nan"}){
  std::istringstream in(std::string("version=9\nlaser=0\nreticle_mode=2\nreticle_restore_mode=")+invalid+"\n");
  auto s=read_settings(in);Hands hands;hands[1].tracked=true;ExtraControls controls;ShortcutPolicy policy;
  policy.update(0,controls,hands,s,true);controls.click[1]=true;policy.update(.1,controls,hands,s,true);
  assert(s.laser&&s.reticle_mode==ReticleMode::Ingame);
 }
 std::filesystem::remove(path);
}
