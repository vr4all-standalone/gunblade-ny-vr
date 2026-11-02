// SPDX-License-Identifier: GPL-2.0-or-later
// Isolated research harness: production transaction + production button latch.
#include "../native/game_options.h"
#include "../native/input_policy.h"
#include "../native/shared_runtime.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <unordered_map>
using namespace gunblade;
int main(int argc,char** argv) {
    if(argc!=4)return 2;
    const std::filesystem::path root=argv[1];
    const bool queue=std::string(argv[2])=="queue";
    GameOptionRequest request{1,queue?GameOption::Difficulty:static_cast<GameOption>(std::stoul(argv[2])),static_cast<uint32_t>(std::stoul(argv[3]))};
    GameOptionsTransaction tx;OperatorLatch latch;
    SharedRuntime runtime;runtime.pause_requested=true;uint32_t original_writes{};
    std::ofstream trace(root/"transaction.tsv");
    const auto began=std::chrono::steady_clock::now();
    for(unsigned sequence=0;sequence<8000;sequence++) {
        const auto input=root/("observe-"+std::to_string(sequence)+".bin");
        while(!std::filesystem::exists(input)) {
            if(std::chrono::steady_clock::now()-began>std::chrono::seconds(180))return 3;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::unordered_map<uint32_t,uint32_t> memory;
        {std::ifstream in(input,std::ios::binary);uint32_t a,v;
            while(in.read(reinterpret_cast<char*>(&a),4)&&in.read(reinterpret_cast<char*>(&v),4))memory[a]=v;}
        auto read=[&](uint32_t a){auto i=memory.find(a);return i==memory.end()?0:i->second;};
        const auto state=read_game_state(read);const auto options=read_game_options(read);
        uint16_t edge{},buttons{};GameOptionsStatus status_value;GameOptionsError error;
        if(queue) {
            if(sequence==0) {
                runtime.game_options=options;GameOptions desired=options;
                desired.difficulty=7;desired.shifting_difficulty=0;desired.player_life=7;desired.attract_sound=0;
                assert(runtime.begin_game_options(desired));assert(runtime.game_options_count==4);
                assert(runtime.game_options_busy&&!runtime.engine_pause_requested());original_writes=read(0x54c0dc);
            }
            runtime.tick_game_options(read,state);buttons=runtime.operator_latched;
            status_value=runtime.game_options_status;error=runtime.game_options_error;
            if(read(0x54c0dc)<original_writes+4)assert(runtime.game_options_busy&&!runtime.engine_pause_requested());
        }else {
            edge=tx.tick(read,state,request);buttons=latch.update(edge);status_value=tx.status();error=tx.error();
        }
        trace<<sequence<<'\t'<<state.mode<<'\t'<<state.submode<<'\t'<<state.service_menu<<'\t'<<read(0x54bda4)
             <<'\t'<<options.value(request.option)<<'\t'<<read(0x54c0dc)<<'\t'<<edge<<'\t'<<buttons
             <<'\t'<<int(status_value)<<'\t'<<int(error);
        if(queue)trace<<'\t'<<runtime.game_options_index<<'\t'<<runtime.game_options_busy<<'\t'<<runtime.engine_pause_requested()
                      <<'\t'<<options.difficulty<<'\t'<<options.shifting_difficulty<<'\t'<<options.player_life<<'\t'<<options.attract_sound;
        trace<<'\n';trace.flush();
        const int status=(queue?(!runtime.game_options_busy&&status_value==GameOptionsStatus::Complete):status_value==GameOptionsStatus::Complete)?1:status_value==GameOptionsStatus::Failed?2:0;
        const auto temp=root/("control-"+std::to_string(sequence)+".tmp");
        {std::ofstream out(temp);out<<buttons<<' '<<status;}
        std::filesystem::rename(temp,root/("control-"+std::to_string(sequence)+".txt"));
        if(status){if(queue&&status==1){assert(read(0x54c0dc)==original_writes+4);assert(options.difficulty==7&&options.shifting_difficulty==0&&options.player_life==7&&options.attract_sound==0);assert(runtime.engine_pause_requested());}
            std::cout<<"transaction status "<<int(status_value)<<", error "<<int(error)<<", native ticks "<<sequence+1<<", queue "<<queue<<'\n';return status==1?0:1;}
    }
    return 4;
}
