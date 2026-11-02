#include "../native/settings_io.h"
#include "../native/world_presets.h"
#include <cassert>
#include <sstream>
#include <cmath>
using namespace gunblade;
bool near(float a,float b){return std::abs(a-b)<.000001f;}
int main(){
 Settings s;assert(s.world_preset==WorldPreset::Normal&&s.laser_thickness==0);
 apply_world_preset(s,WorldPreset::Small,Settings{}.world_scale);assert(near(s.world_scale,.06f));
 for(int i=0;i<10;i++){apply_world_preset(s,WorldPreset::Large,.08f);assert(near(s.world_scale,.10f));apply_world_preset(s,WorldPreset::Small,.08f);assert(near(s.world_scale,.06f));}
 assert(s.wider_view&&s.hud_size==1&&s.reticle_min_size==.22f);
 set_custom_world_scale(s,.123f,.08f);assert(s.world_preset==WorldPreset::Custom&&near(s.world_scale,.123f));s.laser_thickness=2;
 std::ostringstream saved;write_settings(saved,s);std::istringstream load(saved.str());auto cold=read_settings(load);assert(cold.world_preset==WorldPreset::Custom&&near(cold.world_scale,.123f)&&cold.laser_thickness==2);
 std::istringstream legacy("version=6\nworld_scale=0.123\ngun_angle=17\np1_hand=1\nreticle_custom_min_size=.33\n");auto old=read_settings(legacy);assert(old.world_preset==WorldPreset::Custom&&near(old.world_scale,.123f)&&old.gun_angle==17&&old.p1_hand==1&&old.reticle_custom_min_size==.33f);
 std::istringstream normal("version=7\nworld_preset=1\nworld_scale=.19\n");assert(near(read_settings(normal).world_scale,.19f));
 std::istringstream bad("version=7\nworld_preset=999\nworld_scale=.123\nlaser_thickness=nan\n");auto invalid=read_settings(bad);assert(invalid.world_preset==WorldPreset::Custom&&near(invalid.world_scale,.123f)&&invalid.laser_thickness==0);
 assert(near(world_preset_scale(WorldPreset::Large,.10f,.08f),.125f));assert(near(world_preset_scale(WorldPreset::Custom,.10f,.123f),.123f));
 set_custom_world_scale(s,.08f,.08f);assert(s.world_preset==WorldPreset::Normal);
 assert(near(laser_half_width(1,false),.0016f)&&near(laser_half_width(0,false),.0008f)&&near(laser_half_width(2,false),.004f));
 assert(near(laser_half_width(2,true),.004f)&&near(laser_half_width(999,false),.0008f));
 assert(near(laser_half_width(0,true),.0008f)&&near(laser_half_width(1,true),.0016f));
 std::istringstream migration("version=7\nlaser_thickness=2\nworld_preset=2\nworld_scale=.067\nwider_view=1\n");
 auto trial=read_settings(migration);assert(trial.laser_thickness==0&&near(trial.world_scale,.067f)&&trial.wider_view&&!trial.wider_view_fade);
 trial.wider_view_fade=true;trial.laser_thickness=0;std::ostringstream faded;write_settings(faded,trial);
 assert(faded.str().find("world_preset=")==std::string::npos);
 std::istringstream faded_load(faded.str());auto restored=read_settings(faded_load);
 assert(restored.wider_view&&restored.wider_view_fade&&restored.laser_thickness==0&&near(restored.world_scale,.067f));
 std::istringstream bad_fade("version=8\nwider_view=1\nwider_view_fade=nan\nlaser_thickness=99\n");
 auto safe=read_settings(bad_fade);assert(safe.wider_view&&!safe.wider_view_fade&&safe.laser_thickness==0);
 std::istringstream off_fade("version=8\nwider_view=0\nwider_view_fade=1\n");assert(!read_settings(off_fade).wider_view_fade);
 assert(near(laser_half_width(3,false),.008f)&&near(laser_half_width(3,true),.008f));
 Settings ten;ten.laser_thickness=3;ten.laser=false;std::ostringstream ten_saved;write_settings(ten_saved,ten);
 std::istringstream ten_load(ten_saved.str());auto ten_cold=read_settings(ten_load);assert(!ten_cold.laser&&ten_cold.laser_thickness==3);
 std::istringstream previous_width("version=8\nlaser=0\nlaser_thickness=2\nworld_scale=.123\nwider_view_fade=1\n");
 auto kept=read_settings(previous_width);assert(!kept.laser&&kept.laser_thickness==2&&near(kept.world_scale,.123f)&&kept.wider_view_fade);
}
