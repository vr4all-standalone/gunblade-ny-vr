// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/cabinet_io.h"
#include <cassert>
#include <cmath>
#include <limits>
int main() {
    using namespace gunblade;
    Cabinet input;
    assert(adc(0,input)==0xb1 && adc(1,input)==0x50);
    assert(adc(2,input)==0x5f && adc(3,input)==0x5f);
    assert(adc(4,input)==0xff);
    input.x={0x69,0x96};input.y={0x11,0xae};
    assert(adc(0,input)==0x69 && adc(1,input)==0x96);
    assert(adc(2,input)==0x11 && adc(3,input)==0xae);
    assert(in0(0xff,input)==0xff && in1(0xff,input)==0xff);
    input.trigger={true,false};assert(in1(0xff,input)==0xfe);
    input.trigger={false,true};assert(in1(0xff,input)==0xfd);
    input.trigger={true,true};assert(in1(0xa7,input)==0xa4);
    input.coins=3;input.starts=3;input.test=input.service=true;
    assert(in0(0xff,input)==0xc0);
    assert(in0(0x80,input)==0x80); // Never fabricate EEPROM/other native bits.
    auto a=map_aim(0,0.,0.);assert(a.valid && a.x==0x69 && a.y==0x11);
    auto b=map_aim(1,1.,1.);assert(b.valid && b.x==0x96 && b.y==0xae);
    auto center=map_aim(0,.5,.5);assert(center.x==0xb4 && center.y==0x60);
    auto clamped=map_aim(0,-2.,3.);assert(clamped.valid && clamped.x==0x69 && clamped.y==0xae);
    assert(!map_aim(2,.5,.5).valid);
    assert(!map_aim(0,std::numeric_limits<double>::quiet_NaN(),.5).valid);
    assert(!map_aim(0,.5,std::numeric_limits<double>::infinity()).valid);
    // Calibration may reverse axes; measured native calibration owns the values.
    Calibration reversed{255,105,174,17};
    auto r=map_aim(0,0.,1.,reversed);assert(r.valid && r.x==255 && r.y==17);
}
