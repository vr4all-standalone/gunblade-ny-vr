#include "../native/aim_policy.h"
#include <cassert>
int main(){
    using namespace gunblade;
    std::array<NativeAimCalibration,2> calibration;
    for(int p=0;p<2;p++){auto& c=calibration[p];c.valid=c.screen_inverse_valid=true;c.center_x=c.center_y=128;c.angle_units_x=(37.5f/128)*183;c.angle_units_y=(27.f/-128)*183;c.origin_x=p==0?-3.2f:3.2f;c.origin_y=-1.5f;}
    Hands hands;for(auto& h:hands){h.tracked=true;h.aim={.5,.5,true,true};}
    hands[0].aim={.1,.2,true,true};hands[1].aim={.8,.9,true,true};
    CabinetInput input;input.hand={1,0};input.cabinet.trigger={true,true};
    NativeAimPolicy aim_policy;aim_policy.apply(input,hands,calibration);
    for(int p=0;p<2;p++){const auto& aim=hands[input.hand[p]].aim;auto expected=native_screen_to_adc(p,aim.u,aim.v,calibration[p]);assert(input.cabinet.x[p]==expected.x && input.cabinet.y[p]==expected.y && input.cabinet.trigger[p]);}
    calibration[0].valid=false;
    aim_policy.apply(input,hands,calibration);assert(!input.cabinet.trigger[0] && input.cabinet.trigger[1]);
    hands[0].tracked=false;aim_policy.apply(input,hands,calibration);assert(!input.cabinet.trigger[1]);

    // Calibration loss is a release boundary, just like lost tracking. A held
    // pull cannot resume firing merely because its native mapping recovers.
    InputPolicy cabinet;NativeAimPolicy mapping;NativeState native;native.valid=native.start_ready=true;native.joined_mask=1;
    Settings settings;settings.p1_hand=1;for(auto& hand:hands){hand.tracked=true;hand.trigger=false;hand.aim={.5,.5,true,true};}
    cabinet.update(0,hands,native,settings,true,false);
    hands[1].trigger=true;auto lost=cabinet.update(.01,hands,native,settings,true,false);
    calibration[0].valid=false;mapping.apply(lost,hands,calibration);assert(!lost.cabinet.trigger[0]);
    calibration[0].valid=true;auto recovered=cabinet.update(.02,hands,native,settings,true,false);
    mapping.apply(recovered,hands,calibration);assert(!recovered.cabinet.trigger[0]);
    hands[1].trigger=false;auto release=cabinet.update(.03,hands,native,settings,true,false);mapping.apply(release,hands,calibration);
    hands[1].trigger=true;auto fresh=cabinet.update(.04,hands,native,settings,true,false);mapping.apply(fresh,hands,calibration);
    assert(fresh.cabinet.trigger[0]);
    mapping.suspend();fresh.accepted_shots[0]=1;mapping.apply(fresh,hands,calibration);
    assert(!fresh.cabinet.trigger[0] && fresh.accepted_shots[0]==0);
}
