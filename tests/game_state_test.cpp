// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/game_state.h"
#include "../native/hud_size.h"
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
using namespace gunblade;
struct Memory {
    std::unordered_map<uint32_t,uint32_t> words{
        {0x075d64,0x8c203000},{0x075d68,0x00532d54},
        {0x0ed8f8,0x58a50980},{0x06d414,0x90a16014},
        {0x0f0bb0,0x0054bdc4},{0x0f1bcc,0x000f0ba0},
        {0x5329f0,3}
    };
    uint32_t operator()(uint32_t address) const {
        assert((address&3)==0);
        auto it=words.find(address);return it==words.end()?0:it->second;
    }
    GameState state() const { return read_game_state(*this); }
};
std::vector<uint8_t> bytes(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);assert(input);
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
uint32_t word(const std::vector<uint8_t>& data,uint32_t offset) {
    assert(offset+4<=data.size());
    return uint32_t(data[offset]) | (uint32_t(data[offset+1])<<8) |
           (uint32_t(data[offset+2])<<16) | (uint32_t(data[offset+3])<<24);
}
std::vector<std::string> columns(const std::string& row) {
    std::istringstream input(row);std::vector<std::string> values;std::string value;
    while(std::getline(input,value,'\t'))values.push_back(value);
    return values;
}
template<class Observe> void trace(const std::filesystem::path& path,
                                 const std::vector<uint8_t>& rom,Observe observe) {
    std::ifstream input(path);assert(input);std::string line;
    std::getline(input,line);const auto header=columns(line);
    while(std::getline(input,line)) {
        const auto values=columns(line);assert(values.size()==header.size());
        std::unordered_map<uint32_t,uint32_t> ram;
        for(size_t i=1;i<header.size();i++) {
            const auto owner=header[i].rfind("owner",0)==0;
            const uint32_t address=owner?0x532a40+0x48*std::stoul(header[i].substr(5))+0x2c:
                                         std::stoul(header[i],nullptr,16);
            ram[address]=std::stoul(values[i],nullptr,owner?10:16);
        }
        auto read=[&](uint32_t address) {
            if(address<rom.size())return word(rom,address);
            const auto it=ram.find(address);return it==ram.end()?0:it->second;
        };
        observe(read);
    }
}
void native_evidence(const std::filesystem::path& program,const std::filesystem::path& root) {
    const auto rom=bytes(program);assert(rom.size()==0x200000);
    auto snapshot=[&](const char* file) {
        const auto ram=bytes(root/file);assert(ram.size()==0x100000);
        auto read=[&](uint32_t address) {
            return address<rom.size()?word(rom,address):word(ram,address-0x500000);
        };
        const auto state=read_game_state(read);assert(state.valid);return state;
    };
    auto state=snapshot("state-observe/ram-0000.bin");assert(state.mode==11 && !state.ready);
    state=snapshot("state-observe/ram-0130.bin");assert(state.mode==0 && state.ready);
    state=snapshot("state-observe/ram-0170.bin");assert(state.mode==2 && state.submode==1);
    assert(state.players[0].alive() && !state.players[1].joined());assert(state.playing&&!hud_active_stage(state));
    state=snapshot("state-observe/ram-0310.bin");assert(state.submode==6 && state.players[1].alive());assert(state.playing&&!hud_active_stage(state));
    state=snapshot("state-observe/ram-0350.bin");assert(state.submode==2 && state.players[0].health==1);assert(hud_active_stage(state));
    state=snapshot("state-observe/ram-0450.bin");assert(state.players[0].health>0.666f && state.players[0].health<0.667f);
    state=snapshot("state-observe/ram-0480.bin");assert(state.players[0].health>0.333f && state.players[0].health<0.334f);
    state=snapshot("baseline-aim-set/workram-025.bin");assert(state.service && state.gun_calibration && state.service_menu==7);
    state=snapshot("baseline-input-test/workram-025.bin");assert(state.service && !state.gun_calibration && state.service_menu==1);
    ShotTracker tracker;GameState final;unsigned observed=0;
    trace(root/"state-adapter/gameplay-verified/state.tsv",rom,[&](auto read) {
        auto next=read_game_state(read);assert(next.valid);next.session=1;
        tracker.update(next,read);final=next;observed++;
    });
    assert(observed==2934 && final.shot_counters_valid);
    assert(final.players[0].shots==66 && final.players[1].shots==55);
    unsigned phases=0;bool factory_seen=false;
    std::array<NativeAimCalibration,2> calibration{};
    trace(root/"state-adapter/aim-verified/state.tsv",rom,[&](auto read) {
        const auto next=read_game_state(read);assert(next.valid);
        for(unsigned player=0;player<2;player++)calibration[player]=read_native_calibration(read,player);
        if(calibration[0].factory && calibration[1].factory)factory_seen=true;
        if(next.gun_calibration && next.service_busy==1)phases|=1;
        if(next.gun_calibration && next.service_busy==2)phases|=2;
        if(phases==3 && next.gun_calibration && next.service_busy==0)phases|=4;
    });
    assert(phases==7 && factory_seen);
    for(const auto& aim:calibration)assert(aim.valid && !aim.factory && aim.screen_inverse_valid && aim.top==17 && aim.bottom==174 && aim.center_y==65);
    assert(calibration[0].left==105 && calibration[0].right==255 && calibration[0].center_x==145);
    assert(calibration[1].left==0 && calibration[1].right==150 && calibration[1].center_x==109);
    std::array<std::set<uint16_t>,2> positions;
    uint32_t previous_raw=0xffffffff;unsigned stable=0,roundtrips=0;double maximum_error=0;
    trace(root/"state-adapter/aim-sweep/state.tsv",rom,[&](auto read) {
        const auto raw=read(0x532ce0);stable=raw==previous_raw?stable+1:0;previous_raw=raw;
        if(stable<2 || read(0x532964)!=0x71770 || read(0x540088)<10)return;
        for(unsigned player=0;player<2;player++) {
            const auto x=static_cast<uint8_t>(raw>>(player*8)),y=static_cast<uint8_t>(raw>>(16+player*8));
            const auto aim=read_native_calibration(read,player);assert(aim.valid && aim.screen_inverse_valid);
            const auto expected=native_aim_point(aim,x,y);assert(expected.valid);
            const auto native_x=std::bit_cast<float>(read(0x54011c+player*64));
            const auto native_y=std::bit_cast<float>(read(0x540120+player*64));
            assert(std::bit_cast<float>(read(0x540124+player*64))==10);
            const auto error=std::max(std::abs(expected.x-native_x),std::abs(expected.y-native_y));
            maximum_error=std::max(maximum_error,error);assert(error<0.000001);
            positions[player].insert(uint16_t(x)|(uint16_t(y)<<8));
            const auto u=(248+60*double(native_x))/495,v=(192-60*double(native_y))/383;
            if(u>=0 && u<=1 && v>=0 && v<=1) {
                const auto inverse=native_screen_to_adc(player,u,v,aim);
                assert(inverse.valid && inverse.x==x && inverse.y==y);roundtrips++;
            }
        }
    });
    assert(positions[0].size()==81 && positions[1].size()==81 && roundtrips>100);
    std::cout<<"Native evidence: 9 RAM snapshots, "<<observed<<" gameplay frames, shots 66/55, committed calibration, "
             <<positions[0].size()+positions[1].size()<<" aim positions, "<<roundtrips
             <<" ADC roundtrips, max world error "<<maximum_error<<'\n';
}
int main(int argc,char** argv) {
    // A missing/mismatched native revision cannot enable cabinet input.
    Memory memory;auto initial=memory.state();
    assert(initial.valid && initial.ready && !initial.playing);
    for(auto address:{0x075d64u,0x075d68u,0x0ed8f8u,0x06d414u,0x0f0bb0u,0x0f1bccu}) {
        auto bad=memory;bad.words[address]^=1;const auto state=bad.state();
        assert(!state.valid && !state.ready && !state.playing && !state.service);
        assert(!state.players[0].joined() && !state.players[1].alive());
    }
    // Match the Start dispatch table, including its mode-2 lockout.
    for(uint32_t mode=0;mode<16;mode++) {
        memory.words[0x532d54]=mode;
        auto state=memory.state();
        assert(state.valid);
        assert(state.ready==(mode==0 || mode==1 || mode==2 || mode==8 || mode==9));
        assert(state.playing==(mode==2));assert(state.service==(mode==10));
    }
    // Read real native dispatch fields: selection and intro remain playing for input,
    // but only active stage is allowed to resize HUD.
    memory.words[0x532d54]=2;
    for(uint32_t submode:{1u,6u,2u}){memory.words[0x532d48]=submode;const auto phase=memory.state();assert(phase.valid&&phase.playing);assert(hud_active_stage(phase)==(submode==2));}
    memory.words[0x532d48]=0;
    memory.words[0x532d54]=2;memory.words[0x5401d0]=1;
    assert(!memory.state().ready);
    memory.words[0x5401d0]=0;
    // Joined bits occupy only the low byte; continue/dead flags are not players.
    memory.words[0x532d58]=0xabcdef01;memory.words[0x540114]=3;memory.words[0x540154]=3;
    auto solo=memory.state();
    assert(solo.players[0].joined() && solo.players[0].alive());
    assert(solo.players[0].health==1.0f);
    assert(!solo.players[1].joined() && !solo.players[1].alive());
    memory.words[0x532d58]=3;memory.words[0x540114]=1;
    auto both=memory.state();assert(both.players[1].alive());
    assert(both.players[0].health>0.333f && both.players[0].health<0.334f);
    memory.words[0x540114]=0;assert(!memory.state().players[0].alive());
    memory.words[0x540114]=0xffffffff;assert(!memory.state().players[0].alive());
    // Lives normalize against the configured initial life count, not a guessed 3.
    memory.words[0x5329f0]=5;memory.words[0x540114]=2;
    assert(memory.state().players[0].health==0.4f);
    // Native boot/service never expose stale joined/life data as living players.
    memory.words[0x532d54]=11;assert(!memory.state().ready && !memory.state().players[0].joined());
    memory.words[0x532d54]=10;memory.words[0x532d48]=1;memory.words[0x54bdf8]=7;
    auto menu=memory.state();assert(menu.service && !menu.gun_calibration);
    memory.words[0x532d48]=3;memory.words[0x54bdc4]=2;memory.words[0x54bdc8]=0;
    auto aim=memory.state();
    assert(aim.gun_calibration && aim.service_menu==7 && aim.service_submode==2 && aim.service_busy==1);
    memory.words[0x54bdc4]=0;memory.words[0x54bdc8]=2;
    assert(memory.state().service_submode==0x200 && memory.state().service_busy==2);
    memory.words[0x54bdc8]=0;memory.words[0x54bf20]=1;
    assert(memory.state().service_busy==0); // Pending settings write is not calibration activity.
    memory.words[0x54bdf8]=1;assert(!memory.state().gun_calibration);
    // Input mirrors, held-time counters, and projectile indices are not totals.
    memory.words[0x532d54]=2;memory.words[0x532d48]=2;memory.words[0x532ced]=3;
    memory.words[0x540134]=123;memory.words[0x5329ac]=7;
    const auto unsupported=memory.state();
    assert(!unsupported.counting_shots && !unsupported.shot_counters_valid);
    for(const auto& player:unsupported.players)assert(player.shots==0 && player.ammo==0 && player.capacity==0 && player.weapon==0);

    // Accepted projectile creation, not physical pulls, produces feedback.
    ShotTracker tracker;memory.words[0x532964]=0x71770;
    memory.words[0x540088]=100;memory.words[0x5329ac]=7;
    auto sample=[&](uint64_t session=9) {
        auto state=memory.state();state.session=session;tracker.update(state,memory);return state;
    };
    assert(!sample().shot_counters_valid); // Establish the native cursor baseline.
    memory.words[0x540088]=101;memory.words[0x5329ac]=1;
    memory.words[0x532a40+0x48*7+0x2c]=0;memory.words[0x532a40+0x2c]=1;
    auto fired=sample();
    assert(fired.counting_shots && fired.shot_counters_valid);
    assert(fired.players[0].shots==1 && fired.players[1].shots==1);
    assert(sample().players[0].shots==1); // Duplicate host sampling never repeats a shot.
    memory.words[0x540088]=102;assert(sample().players[0].shots==1);
    // Missing native frames, counter resets and impossible bursts are discarded.
    memory.words[0x540088]=104;memory.words[0x5329ac]=2;
    memory.words[0x532a40+0x48+0x2c]=0;
    auto gap=sample();assert(!gap.shot_counters_valid && gap.players[0].shots==1);
    memory.words[0x540088]=105;memory.words[0x5329ac]=3;
    memory.words[0x532a40+0x48*2+0x2c]=0;
    assert(sample().players[0].shots==2);
    memory.words[0x540088]=0;assert(!sample().shot_counters_valid);
    memory.words[0x540088]=1;memory.words[0x5329ac]=6;
    assert(!sample().shot_counters_valid);
    memory.words[0x540088]=2;memory.words[0x5329ac]=0;
    memory.words[0x532a40+0x48*6+0x2c]=0;memory.words[0x532a40+0x48*7+0x2c]=0;
    auto duplicate_owner=sample();assert(!duplicate_owner.shot_counters_valid && duplicate_owner.players[0].shots==2);
    memory.words[0x540088]=3;memory.words[0x5329ac]=1;memory.words[0x532a40+0x2c]=7;
    assert(!sample().shot_counters_valid);
    memory.words[0x540088]=4;memory.words[0x5329ac]=8;
    assert(!sample().shot_counters_valid); // Corrupt cursor cannot alias a valid slot.
    memory.words[0x540088]=5;memory.words[0x5329ac]=0;
    assert(!sample().shot_counters_valid);
    // A stage/menu transition and a session change establish fresh baselines.
    memory.words[0x532d48]=6;assert(!sample().counting_shots);
    memory.words[0x532d48]=2;memory.words[0x540088]=6;
    assert(!sample().shot_counters_valid);
    memory.words[0x540088]=7;memory.words[0x5329ac]=1;memory.words[0x532a40+0x2c]=0;
    assert(sample().players[0].shots==3);
    auto new_session=sample(10);assert(!new_session.shot_counters_valid && new_session.players[0].shots==0);
    memory.words[0x540088]=8;memory.words[0x5329ac]=2;memory.words[0x532a40+0x48+0x2c]=1;
    assert(sample(10).players[1].shots==1);
    memory.words[0x532d58]=1;memory.words[0x540088]=9;
    assert(!sample(10).shot_counters_valid);
    memory.words[0x540088]=10;memory.words[0x5329ac]=3;memory.words[0x532a40+0x48*2+0x2c]=1;
    assert(!sample(10).shot_counters_valid); // An unjoined owner is not feedback.
    memory.words[0x532964]=0x71d70;assert(!sample(10).shot_counters_valid); // Demo shooter.
    tracker.suspend();memory.words[0x532964]=0x71110;
    assert(sample(10).players[1].shots==0);
    // Native factory Y endpoints have a negative span: preserve its direction.
    memory.words[0x54c0ec]=0x40000101;memory.words[0x54c0f0]=0x80c040c0;
    memory.words[0x54c0f4]=0xffffff80;memory.words[0x54c0f8]=0xc04ccccd;
    memory.words[0x54c0fc]=0xbfc00000;memory.words[0x54c100]=0xc040c040;
    memory.words[0x54c104]=0xffff8080;memory.words[0x54c108]=0x404ccccd;
    memory.words[0x54c10c]=0xbfc00000;
    auto calibration=read_native_calibration(memory,0);
    assert(calibration.valid && calibration.factory && calibration.screen_inverse_valid);
    assert(calibration.left==64 && calibration.right==192 && calibration.top==192 && calibration.bottom==64);
    assert(calibration.center_x==128 && calibration.center_y==128);
    assert(calibration.angle_units_x==53.61328125f && calibration.angle_units_y==-38.6015625f);
    calibration=read_native_calibration(memory,1);
    assert(calibration.valid && calibration.factory && calibration.origin_x>3.19f && calibration.origin_y==-1.5f);
    auto native_point=native_aim_point(read_native_calibration(memory,0),177,95);
    assert(native_point.valid && std::abs(native_point.x-0.039776802)<0.000001 &&
           std::abs(native_point.y-0.082727432)<0.000001 && native_point.z==10);
    auto center=native_screen_to_adc(0,.5,.5,read_native_calibration(memory,0));
    assert(center.valid && center.x==176 && center.y==97);
    center=native_screen_to_adc(1,.5,.5,read_native_calibration(memory,1));
    assert(center.valid && center.x==79 && center.y==97);
    auto corner=native_screen_to_adc(0,0,0,read_native_calibration(memory,0));
    assert(corner.valid && corner.x==114 && corner.y==28);
    corner=native_screen_to_adc(1,1,1,read_native_calibration(memory,1));
    assert(corner.valid && corner.x==142 && corner.y==163);
    // NativeLens normalizes pixel endpoints by width-1/height-1.
    auto endpoint=native_screen_to_adc(0,1,.8,read_native_calibration(memory,0));
    assert(endpoint.valid && endpoint.x==244 && endpoint.y==136);
    assert(!native_screen_to_adc(2,.5,.5,calibration).valid);
    assert(!native_screen_to_adc(0,NAN,.5,calibration).valid);
    assert(!native_screen_to_adc(0,.5,.5,{}).valid);
    auto corrupt_calibration=calibration;corrupt_calibration.angle_units_x=NAN;
    assert(!native_screen_to_adc(0,.5,.5,corrupt_calibration).valid);
    // Native operator calibration stores an offset center, not the midpoint.
    memory.words[0x54c0ec]=0xae000101;memory.words[0x54c0f0]=0x91ff6911;
    memory.words[0x54c0f4]=0xffffff5f;memory.words[0x54c0f8]=0xbfec7e28;
    memory.words[0x54c0fc]=0xbfa91d15;
    calibration=read_native_calibration(memory,0);
    assert(calibration.valid && !calibration.factory && calibration.center_x==145);
    assert(calibration.left==105 && calibration.right==255 && calibration.top==17 && calibration.bottom==174);
    memory.words[0x54c0f0]=0x91696911;assert(!read_native_calibration(memory,0).valid);
    memory.words[0x54c0f0]=0x91ff6911;memory.words[0x54c0f8]=0x7fc00000;
    assert(!read_native_calibration(memory,0).valid);
    assert(!read_native_calibration(memory,2).valid);
    memory.words[0x075d64]=0;assert(!read_native_calibration(memory,1).valid);
    if(argc==4 && std::string(argv[1])=="--native-evidence")native_evidence(argv[2],argv[3]);
    else assert(argc==1);
}
