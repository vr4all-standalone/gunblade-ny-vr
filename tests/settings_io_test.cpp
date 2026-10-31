#include "../native/settings_io.h"
#include <cassert>
#include <sstream>
using namespace gunblade;
int main(){
    assert(Settings{}.p1_hand==0);
    assert(Settings{}.wider_view);
    // A fresh install uses the player's accepted Quest presentation.
    std::ostringstream fresh_defaults;write_settings(fresh_defaults,Settings{});
    assert(fresh_defaults.str().find("world_scale=0.08\n")!=std::string::npos);
    assert(fresh_defaults.str().find("reticle_min_size=0.22\n")!=std::string::npos);
    assert(fresh_defaults.str().find("reticle_shrink_distance=6.7\n")!=std::string::npos);
    for(const char* partial:{"version=6\n","version=6\nworld_scale=nan\n","version=6\nworld_scale=4\n"}){
        std::istringstream input(partial);assert(read_settings(input).world_scale==.08f);
    }
    std::istringstream explicit_scale("version=6\nworld_scale=0.805\n");
    auto personal=read_settings(explicit_scale);assert(personal.world_scale==.805f);
    std::ostringstream personal_saved;write_settings(personal_saved,personal);
    std::istringstream personal_reload(personal_saved.str());assert(read_settings(personal_reload).world_scale==.805f);
    std::istringstream legacy("version=1\nwider_view=0\nworld_scale=0.25\n");
    auto migrated=read_settings(legacy);
    assert(migrated.wider_view && migrated.world_scale==.25f);
    std::istringstream fresh("laser=0\n");
    assert(read_settings(fresh).wider_view);
    std::istringstream current("wider_view=0\nversion=2\n");
    auto selected=read_settings(current);assert(!selected.wider_view);
    std::ostringstream output;write_settings(output,selected);
    assert(output.str().find("version=6\n")==0);
    std::istringstream roundtrip(output.str());assert(!read_settings(roundtrip).wider_view);
    std::istringstream invalid("version=2\nwider_view=nan\n");
    assert(read_settings(invalid).wider_view);
    std::istringstream old_hands("version=2\np1_hand=1\nwider_view=0\nworld_scale=0.25\n");
    auto hand_migration=read_settings(old_hands);
    assert(hand_migration.p1_hand==0&&!hand_migration.wider_view&&hand_migration.world_scale==.25f);
    // Once upgraded, an explicit reversed assignment survives later launches.
    hand_migration.p1_hand=1;
    std::ostringstream reversed;write_settings(reversed,hand_migration);
    std::istringstream reloaded(reversed.str());
    assert(read_settings(reloaded).p1_hand==1);
    std::istringstream current_hands("p1_hand=1\nversion=6\n");
    bool upgraded=true;assert(read_settings(current_hands,&upgraded).p1_hand==1&&!upgraded);
    std::istringstream needs_upgrade("version=2\np1_hand=1\n");
    assert(read_settings(needs_upgrade,&upgraded).p1_hand==0&&upgraded);
    std::istringstream old_angle("version=3\np1_hand=1\nwider_view=0\ngun_angle=17\ngun=2\nworld_scale=0.25\n");
    auto new_baseline=read_settings(old_angle,&upgraded);
    assert(new_baseline.gun_angle==0&&new_baseline.gun==0&&upgraded);
    assert(new_baseline.p1_hand==1&&!new_baseline.wider_view&&new_baseline.world_scale==.25f);
    std::istringstream later_angle("version=6\ngun_angle=13\ngun=1\np1_hand=1\n");
    auto later=read_settings(later_angle,&upgraded);
    assert(later.gun_angle==13&&later.gun==1&&later.p1_hand==1&&!upgraded);
    std::ostringstream newer;write_settings(newer,later);
    assert(newer.str().find("version=6\n")==0);
    std::istringstream recheck(newer.str());assert(read_settings(recheck).gun_angle==13);
    std::istringstream previous("version=4\ngun_angle=13\np1_hand=1\nwider_view=0\npanel_distance=2.4\n");
    auto knobs=read_settings(previous,&upgraded);
    assert(upgraded&&knobs.gun_angle==13&&knobs.p1_hand==1&&!knobs.wider_view);
    assert(knobs.reticle_min_size==.22f&&knobs.reticle_shrink_distance==2.4f);
    knobs.reticle_min_size=.55f;knobs.reticle_shrink_distance=4.2f;
    std::ostringstream tuned;write_settings(tuned,knobs);std::istringstream again(tuned.str());
    auto restored=read_settings(again,&upgraded);
    assert(!upgraded&&restored.reticle_min_size==.55f&&restored.reticle_shrink_distance==4.2f&&restored.gun_angle==13);
    std::istringstream bad_knobs("version=5\nreticle_min_size=nan\nreticle_shrink_distance=999\npanel_distance=2.4\n");
    auto clean=read_settings(bad_knobs);assert(clean.reticle_min_size==.22f&&clean.reticle_shrink_distance==2.4f);
    std::istringstream version5("version=5\nreticle_min_size=0.55\nreticle_shrink_distance=4.2\ngun_angle=13\np1_hand=1\nwider_view=0\n");
    auto modes=read_settings(version5,&upgraded);
    assert(upgraded&&modes.reticle_mode==ReticleMode::Ingame&&modes.reticle_min_size==.55f&&modes.reticle_shrink_distance==4.2f&&modes.gun_angle==13&&modes.p1_hand==1&&!modes.wider_view);
    for(auto mode:{ReticleMode::Ingame,ReticleMode::Custom,ReticleMode::None}){
        modes.reticle_mode=mode;std::ostringstream saved;write_settings(saved,modes);std::istringstream input(saved.str());
        auto loaded=read_settings(input,&upgraded);assert(!upgraded&&loaded.reticle_mode==mode&&loaded.reticle_min_size==.55f);
    }
    for(const char* invalid:{"-1","3","1.5","nan"}){
        std::istringstream input(std::string("version=6\nreticle_mode=")+invalid+"\n");
        assert(read_settings(input).reticle_mode==ReticleMode::Ingame);
    }
}
