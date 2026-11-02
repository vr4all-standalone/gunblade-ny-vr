#include "../native/shared_runtime.h"
#include <cassert>
using namespace gunblade;
int main(){
 SharedRuntime r;r.pause_requested=true;r.game_options={true,true,3,3,3,1};
 assert(r.engine_pause_requested());auto desired=r.game_options;desired.difficulty=7;desired.player_life=7;
 assert(r.begin_game_options(desired)&&r.game_options_busy&&!r.engine_pause_requested());
 assert(r.game_options_count==2&&r.game_options_queue[0].option==GameOption::Difficulty&&r.game_options_queue[1].option==GameOption::PlayerLife);
 // Invalid revision rejects the submitted queue and clears held operator input.
 r.operator_pending=8;r.operator_latched=8;auto bad=[](uint32_t){return 0u;};GameState unavailable;
 r.tick_game_options(bad,unavailable);assert(!r.game_options_busy&&r.engine_pause_requested()&&r.game_options_status==GameOptionsStatus::Failed);
 assert(r.operator_latched==0&&r.operator_pending==0&&r.game_options_count==0);
 // Recovery is possible after the first failure: manual Service must not be
 // erased again on every frame by the completed failed request.
 r.operator_pending=8;r.tick_game_options(bad,unavailable);assert(r.operator_latched==8);
 r.reset();assert(!r.game_options_busy&&r.game_options_count==0&&r.game_options_request.id==0);
}
