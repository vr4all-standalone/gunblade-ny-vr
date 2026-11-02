// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/shared_runtime.h"
#include <cassert>
#include <iostream>
#include <unordered_map>
using namespace gunblade;
struct NativeMemory {
    std::unordered_map<uint32_t,uint32_t> words{
        {0x075d64,0x8c203000},{0x075d68,0x00532d54},
        {0x0ed8f8,0x58a50980},{0x06d414,0x90a16014},
        {0x0f0bb0,0x0054bdc4},{0x0f1bcc,0x000f0ba0},
        {0x0efc10,0x8c086140},{0x0efca4,0x0054c0e4},
        {0x0f01bc,0x0054c0ea},{0x0f1ae8,0x09f1c348},
        {0x00de30,0x90a03000},{0x00de34,0x0054c0dc},
        {0x54c0d0,0x41474553},{0x54c0d4,0x594e4247},
        {0x54c0e0,0xffff0058},{0x54c0e4,0x03000001},
        {0x54c0e8,0x000303ff},{0x54c0ec,0x40000101},
        {0x54bdf8,10},{0x54bda4,8}
    };
    uint32_t operator()(uint32_t a) const {assert(!(a&3));auto i=words.find(a);return i==words.end()?0:i->second;}
};
// A focus failure must clear stale native levels exactly once. Clearing on
// every later tick would silently prevent the original Service recovery path.
void queued_focus_loss_allows_native_recovery() {
    NativeMemory memory;SharedRuntime runtime;
    runtime.pause_requested=true;assert(runtime.engine_pause_requested());
    runtime.game_options=read_game_options(memory);
    GameOptions desired=runtime.game_options;desired.difficulty=7;desired.shifting_difficulty=0;desired.player_life=7;desired.attract_sound=0;
    assert(runtime.begin_game_options(desired));
    assert(runtime.game_options_count==4&&runtime.game_options_busy&&!runtime.engine_pause_requested());
    assert(runtime.game_options_queue[0].option==GameOption::Difficulty&&runtime.game_options_queue[0].value==7);
    assert(runtime.game_options_queue[1].option==GameOption::ShiftingDifficulty&&runtime.game_options_queue[1].value==0);
    assert(runtime.game_options_queue[2].option==GameOption::PlayerLife&&runtime.game_options_queue[2].value==7);
    assert(runtime.game_options_queue[3].option==GameOption::AttractSound&&runtime.game_options_queue[3].value==0);
    runtime.tick_game_options(memory,read_game_state(memory));
    assert(runtime.operator_latched==4&&runtime.game_options_busy);
    runtime.input.cabinet.starts=3;runtime.engine_input.cabinet.starts=3;
    runtime.operator_pending=0x3c;runtime.game_options_focused=false;
    memory.words[0x532d54]=10;memory.words[0x532d48]=1;
    runtime.tick_game_options(memory,read_game_state(memory));
    assert(runtime.game_options_status==GameOptionsStatus::Failed&&runtime.game_options_error==GameOptionsError::Interrupted);
    assert(!runtime.game_options_busy&&runtime.engine_pause_requested()&&runtime.game_options_count==0);
    assert(runtime.operator_latched==0&&runtime.operator_pending==0);
    assert(runtime.input.cabinet.starts==0&&runtime.engine_input.cabinet.starts==0);
    // Original manual Service remains usable for recovery after failure.
    runtime.game_options_focused=true;runtime.operator_pending=8;
    runtime.tick_game_options(memory,read_game_state(memory));
    assert(runtime.operator_latched==8&&runtime.operator_pending==0);
    for(int tick=0;tick<11;tick++){runtime.tick_game_options(memory,read_game_state(memory));assert(runtime.operator_latched==8);}
    runtime.tick_game_options(memory,read_game_state(memory));assert(runtime.operator_latched==0);
    assert(runtime.game_options_status==GameOptionsStatus::Failed&&!runtime.game_options_busy);
}
int main(){queued_focus_loss_allows_native_recovery();std::cout<<"game_options_queue: passed\n";}
