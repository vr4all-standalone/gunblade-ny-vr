#include "../native/reticle_size.h"
#include "../native/settings_io.h"
#include <cassert>
#include <cmath>
#include <sstream>
using namespace gunblade;
static bool near(double a,double b){return std::abs(a-b)<1e-5;}
int main(){
    Settings s;
    assert(near(s.reticle_min_size,.22)&&near(s.reticle_custom_min_size,.22));
    assert(near(s.reticle_max_size,1)&&near(s.reticle_custom_max_size,1));
    s.reticle_min_size=.4f;s.reticle_max_size=1.5f;
    s.reticle_custom_min_size=.2f;s.reticle_custom_max_size=.75f;
    s.reticle_mode=ReticleMode::Ingame;
    assert(near(reticle_size_reference(1,1.8,reticle_size_for_mode(s)),2.7));
    s.reticle_mode=ReticleMode::Custom;
    assert(near(reticle_size_reference(1,1.8,reticle_size_for_mode(s)),1.35));
    std::ostringstream output;write_settings(output,s);std::istringstream input(output.str());
    auto saved=read_settings(input);
    assert(near(saved.reticle_min_size,.4)&&near(saved.reticle_max_size,1.5));
    assert(near(saved.reticle_custom_min_size,.2)&&near(saved.reticle_custom_max_size,.75));
    std::istringstream old("reticle_min_size=.55\nreticle_shrink_distance=6.7\n");
    auto migrated=read_settings(old);
    assert(near(migrated.reticle_custom_min_size,.55));
    std::istringstream bad("reticle_max_size=nan\nreticle_custom_max_size=3.1\nreticle_custom_min_size=.01\n");
    auto safe=read_settings(bad);
    assert(near(safe.reticle_max_size,1)&&near(safe.reticle_custom_max_size,1));
    assert(near(safe.reticle_custom_min_size,.22));
}
