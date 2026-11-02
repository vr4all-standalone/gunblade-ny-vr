// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/game_options.h"
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>
using namespace gunblade;
struct Memory {
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
    void byte(uint32_t a,uint32_t v){auto& w=words[a&~3u];auto s=(a&3)*8;w=(w&~(255u<<s))|(v<<s);}
    GameState state() const{return read_game_state(*this);}
};
// These literal values come from the isolated Revision A cabinet cycle, not
// from production helpers. A swapped address or off-by-one range must fail.
void readback_and_revision() {
    Memory m;auto o=read_game_options(m);
    assert(o.valid&&o.eligible&&o.difficulty==3&&o.shifting_difficulty==3&&o.player_life==3&&o.attract_sound==1);
    m.byte(0x54c0e9,7);m.byte(0x54c0e7,0);m.byte(0x54c0ea,1);m.byte(0x54c0e4,0);
    o=read_game_options(m);assert(o.valid&&o.difficulty==7&&o.shifting_difficulty==0&&o.player_life==1&&o.attract_sound==0);
    m.byte(0x54c0ea,0);assert(!read_game_options(m).valid);
    m.byte(0x54c0ea,7);m.words[0xefca4]^=4;assert(!read_game_options(m).valid);
}
void unavailable_and_invalid_do_not_touch_service() {
    Memory m;m.words[0x532d54]=2;
    GameOptionsTransaction tx;
    assert(tx.tick(m,m.state(),{1,GameOption::Difficulty,5})==0);
    assert(!tx.busy()&&tx.status()==GameOptionsStatus::Failed&&tx.error()==GameOptionsError::Unavailable);
    m.words[0x532d54]=0;
    assert(tx.tick(m,m.state(),{2,GameOption::PlayerLife,0})==0);
    assert(tx.error()==GameOptionsError::InvalidValue);
    assert(tx.tick(m,m.state(),{3,GameOption::AttractSound,2})==0);
    assert(tx.error()==GameOptionsError::InvalidValue);
    assert(tx.tick(m,m.state(),{4,GameOption::Difficulty,8})==0);
    assert(tx.error()==GameOptionsError::InvalidValue);
    m.words[0x532d58]=1;
    assert(!read_game_options(m).eligible);
}
void focus_loss_and_timeout_stop_pulses() {
    Memory m;GameOptionsTransaction tx;
    assert(tx.tick(m,m.state(),{1,GameOption::Difficulty,5})==4);
    assert(tx.busy()&&tx.native_service_owned());
    m.words[0x532d54]=10;m.words[0x532d48]=1;
    assert(tx.tick(m,m.state(),{1,GameOption::Difficulty,5},false)==0);
    assert(tx.status()==GameOptionsStatus::Failed&&tx.error()==GameOptionsError::Interrupted&&!tx.native_service_owned());
    for(int n=0;n<100;n++)assert(tx.tick(m,m.state(),{1,GameOption::Difficulty,5})==0);
    m.words[0x532d54]=0;m.words[0x532d48]=0;
    GameOptionsTransaction stalled;
    for(int n=0;n<2000;n++)stalled.tick(m,m.state(),{2,GameOption::Difficulty,5});
    assert(!stalled.busy()&&stalled.error()==GameOptionsError::Timeout);
}
// Feed observed native phases to prove acknowledgement cannot happen just
// because a RAM setting changed; the original EEPROM save and return are gates.
void commit_acknowledgement(bool corrupt_calibration=false) {
    Memory m;GameOptionsTransaction tx;GameOptionRequest req{1,GameOption::Difficulty,5};
    auto tick=[&](){return tx.tick(m,m.state(),req);};
    assert(tick()==4);
    m.words[0x532d54]=10;m.words[0x532d48]=1;m.words[0x54bdf8]=5;
    for(int i=0;i<24;i++)assert(tick()==0); // Native12-frame hold must release.
    uint16_t pulse=tick();assert(pulse==4);
    m.words[0x532d48]=3;m.words[0x54bda4]=3;
    pulse=0;for(int i=0;i<30&&!pulse;i++)pulse=tick();assert(pulse==4);
    m.byte(0x54c0e9,4);
    pulse=0;for(int i=0;i<30&&!pulse;i++)pulse=tick();assert(pulse==4);
    m.byte(0x54c0e9,5);
    pulse=0;for(int i=0;i<30&&!pulse;i++)pulse=tick();assert(pulse==8);
    m.words[0x54bda4]=8;
    pulse=0;for(int i=0;i<30&&!pulse;i++)pulse=tick();assert(pulse==4);
    m.words[0x532d48]=1;m.words[0x54bdf8]=10;
    pulse=0;for(int i=0;i<30&&!pulse;i++)pulse=tick();assert(pulse==4);
    assert(tx.busy());
    m.words[0x532d54]=0;m.words[0x532d48]=0;
    for(int i=0;i<30;i++)tick();
    assert(tx.busy()); // Missing EEPROM write ack.
    if(corrupt_calibration)m.byte(0x54c0f1,99);
    m.words[0x54c0dc]=1;tick();
    if(corrupt_calibration)assert(tx.status()==GameOptionsStatus::Failed&&tx.error()==GameOptionsError::NativeMismatch);
    else assert(tx.status()==GameOptionsStatus::Complete&&!tx.busy()&&tx.request_id()==1);
    for(int i=0;i<100;i++)assert(tick()==0); // Same request cannot replay.
}
void native_evidence(const std::filesystem::path& program,const std::filesystem::path& root) {
    auto bytes=[](const auto& path){std::ifstream in(path,std::ios::binary);assert(in);return std::vector<uint8_t>{std::istreambuf_iterator<char>(in),{}};};
    const auto rom=bytes(program);
    const char* folders[]={"production-difficulty-cold","production-shifting-cold","production-life-cold","production-attract-cold"};
    constexpr uint32_t expected[][4]={{7,3,3,1},{3,0,3,1},{3,3,7,1},{3,3,3,0}};
    for(unsigned index=0;index<4;index++) {
        const auto ram=bytes(root/folders[index]/"ram-019.bin");
        auto read=[&](uint32_t a) {
            const auto& data=a<rom.size()?rom:ram;const auto offset=a<rom.size()?a:a-0x500000;
            assert(offset+4<=data.size());return uint32_t(data[offset])|(uint32_t(data[offset+1])<<8)|(uint32_t(data[offset+2])<<16)|(uint32_t(data[offset+3])<<24);
        };
        const auto o=read_game_options(read);assert(o.valid&&!o.eligible);
        assert(o.difficulty==expected[index][0]&&o.shifting_difficulty==expected[index][1]&&o.player_life==expected[index][2]&&o.attract_sound==expected[index][3]);
    }
}
int main(int argc,char** argv){readback_and_revision();unavailable_and_invalid_do_not_touch_service();focus_loss_and_timeout_stop_pulses();commit_acknowledgement();commit_acknowledgement(true);if(argc==4&&std::string(argv[1])=="--native-evidence")native_evidence(argv[2],argv[3]);std::cout<<"game_options: passed\n";}
