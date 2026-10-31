// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "input_policy.h"
#include "button_latch.h"
namespace gunblade {
class ControllerHelpPolicy {
    bool intro_{true};
public:
    bool show(bool manual=false,bool menu=false,bool service=false) const{return (manual&&menu)||(intro_&&!service&&!menu);}
    void observe(const CabinetInput& input){if(input.cabinet.coins||input.cabinet.starts||input.cabinet.trigger[0]||input.cabinet.trigger[1])intro_=false;}
};
struct Stick {float x{},y{};};
struct ExtraControls {std::array<Stick,2> stick;std::array<bool,2> grip{},click{};};
struct ShortcutResult {bool recenter{},laser{};};
class MenuHoldPolicy {
    ButtonLatch button_;bool armed_{},opened_{};double began_{};
public:
    struct Result {bool toggle{},start{};};
    void suspend(){button_.suspend();armed_=opened_=false;}
    Result update(double now,bool down,bool focused) {
        if(!focused)suspend();
        Result out;
        if(button_.edge(down,focused)){armed_=true;opened_=false;began_=now;}
        if(armed_&&down&&!opened_&&now-began_>=.8){out.toggle=true;opened_=true;}
        if(!down&&armed_){out.start=!opened_;armed_=false;}
        return out;
    }
};
class ServiceGunPolicy {
    std::array<bool,2> blocked_{true,true},pump_blocked_{true,true};bool enabled_{};int layout_{-1};
public:
    void suspend(){enabled_=false;blocked_={true,true};pump_blocked_={true,true};}
    CabinetInput update(const Hands& hands,const Settings& settings,bool enabled) {
        if(!enabled||enabled!=enabled_||settings.p1_hand!=layout_){blocked_={true,true};pump_blocked_={true,true};}
        enabled_=enabled;layout_=settings.p1_hand;
        CabinetInput out;out.hand={layout_,1-layout_};
        for(int p=0;p<2;p++) {
            int h=out.hand[p];const auto& in=hands[h];
            if(!in.trigger)blocked_[h]=false;
            if(!in.pump)pump_blocked_[h]=false;
            if(!in.tracked){blocked_[h]=true;pump_blocked_[h]=true;}
            if(!enabled||!in.tracked)continue;
            if(in.aim.valid&&std::isfinite(in.aim.u)&&std::isfinite(in.aim.v)) {
                const auto aim=map_aim(p,in.aim.u,in.aim.v,settings.calibration[p]);
                if(aim.valid){out.cabinet.x[p]=aim.x;out.cabinet.y[p]=aim.y;out.cabinet.trigger[p]=in.trigger&&!blocked_[h]&&in.aim.inside;}
            }
        }
        return out;
    }
};
class ShortcutPolicy {
    std::array<ButtonLatch,2> click_;
public:
    void suspend(){for(auto& b:click_)b.suspend();}
    ShortcutResult update(double,const ExtraControls& in,const Hands& hands,Settings&,bool enabled) {
        ShortcutResult out;
        for(int h=0;h<2;h++) {
            bool active=enabled&&hands[h].tracked;
            bool edge=click_[h].edge(in.click[h],active);
            if(h==0)out.recenter=edge;else out.laser=edge;
        }
        return out;
    }
};
class OperatorPolicy {
    std::array<ButtonLatch,2> trigger_,start_,pump_;
    bool axes_wait_{true};int previous_{};double repeat_{};
public:
    void suspend(){for(auto* buttons:{&trigger_,&start_,&pump_})for(auto& b:*buttons)b.suspend();axes_wait_=true;previous_=0;}
    uint16_t update(double now,const ExtraControls& axes,const Hands& hands,bool enabled,bool guns=false,int p1_hand=0) {
        if(!enabled)suspend();
        uint16_t result=0;
        for(int h=0;h<2;h++) {
            bool active=enabled&&hands[h].tracked;
            bool trigger=trigger_[h].edge(hands[h].trigger,active),start=start_[h].edge(hands[h].start,active),pump=pump_[h].edge(hands[h].pump,active);
            if((trigger&&!guns)||(start&&!guns))result|=4;
            if(start&&guns)result|=h==p1_hand?0x10:0x20; // Calibration has real P1/P2 Start functions.
            if(pump&&!guns)result|=8;
        }
        float y=0;
        for(int h=0;h<2;h++)if(hands[h].tracked&&std::abs(axes.stick[h].y)>std::abs(y))y=axes.stick[h].y;
        if(enabled&&std::abs(y)<.25f)axes_wait_=false;
        int direction=enabled&&!axes_wait_&&std::abs(y)>.65f?(y>0?1:-1):0;
        if(direction&&(direction!=previous_||now>=repeat_)) {
            // The Model 2 Service switch advances one row in either direction.
            result|=8;repeat_=now+(direction!=previous_?.5:.3);
        }
        previous_=direction;return result;
    }
};
}
