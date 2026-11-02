#include "../native/game_option_edits.h"
#include <cassert>
using namespace gunblade;
int main(){
 GameOptionEdits edits;GameOptions native{true,false,3,3,3,1};
 edits.stage(native,GameOption::Difficulty,5);assert(edits.dirty(native));
 auto desired=edits.merge(native);assert(desired.difficulty==5&&desired.player_life==3&&!desired.eligible);
 native.player_life=7;assert(edits.merge(native).player_life==7); // Unedited external change survives.
 edits.stage(native,GameOption::AttractSound,0);assert(edits.merge(native).attract_sound==0);
 edits.stage(native,GameOption::Difficulty,3);assert(edits.merge(native).difficulty==3&&edits.dirty(native));
 edits.stage(native,GameOption::PlayerLife,0);assert(edits.merge(native).player_life==7); // Invalid rejected.
 auto invalid=native;invalid.valid=false;edits.stage(invalid,GameOption::ShiftingDifficulty,7);
 assert(edits.merge(native).shifting_difficulty==3);
 edits.clear();assert(!edits.dirty(native));
}
