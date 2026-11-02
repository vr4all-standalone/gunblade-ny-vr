/*
    Controller callouts adapted from hotd2-vr xr_labels.cpp.
    Copyright 2026 mikermak. GNU GPL v2 or later.
    Button points are the Confidential Mission sidecars for the same bundled
    declan02530 controller GLBs (asset attribution: assets/models/CREDITS.txt).
*/
#pragma once
#include "shared/xr_panel.h"
#include "controller_help_text.h"
#include <algorithm>
#include <array>

namespace gunblade::controller_help {
class Labels {
#ifdef GUNBLADE_CONTROLLER_LABEL_LAYOUT_TEST
    friend struct LabelLayoutProbe;
#endif
    // The reference's physical sizes: 16 mm plates, 21 mm pitch, 1.2 mm lines.
    static constexpr float Height=.016f,Gap=.025f,Pitch=.021f,StickRise=.026f;
    static constexpr float TriggerGap=.037f; // clear the inward shell when the hand rolls
    static constexpr float LineWidth=.0012f,Dot=.0024f;
    enum Button {Aux,Start,Menu,Trigger,Stick,Grip};
    struct Picture {unsigned texture{};float hx{},hy{};};
    struct Placed {int button{};glm::vec3 centre{},from{},to{};};
    struct Layout {
        glm::mat4 model{0};glm::vec3 eye{};Words words;size_t count{};
        std::array<Picture,6> pictures;std::array<Placed,6> placed;
    };
    std::array<Layout,2> layouts_;
    inline static const glm::vec3 Points[2][6]={
        {{-.0155f,.0067f,.0028f},{-.0049f,.0046f,-.0097f},{.0114f,.0021f,-.0109f},{-.0142f,-.0159f,.0254f},{.0063f,.0112f,.007f},{.017f,-.027f,-.002f}},
        {{.0154f,.0067f,.0028f},{.0048f,.0046f,-.0097f},{0,0,0},{.0142f,-.0159f,.0254f},{-.0065f,.0111f,.007f},{-.017f,-.027f,-.002f}}
    };
    static glm::vec3 eyeOf(const glm::mat4& vp) {
        constexpr int rows[]={0,1,3};glm::mat3 m;glm::vec3 b;
        for(int i=0;i<3;i++){for(int c=0;c<3;c++)m[c][i]=vp[c][rows[i]];b[i]=-vp[3][rows[i]];}
        return glm::inverse(m)*b;
    }
    static void facing(const glm::vec3& at,const glm::vec3& eye,const glm::vec3& fallback,glm::vec3& across,glm::vec3& up,glm::vec3& toEye) {
        toEye=eye-at;toEye=glm::length(toEye)>1e-4f?glm::normalize(toEye):glm::vec3(0,0,1);
        across=glm::cross(glm::vec3(0,1,0),toEye);
        if(glm::length(across)<1e-3f)across=fallback-toEye*glm::dot(fallback,toEye);
        if(glm::length(across)<1e-3f)across=glm::cross(glm::vec3(0,0,1),toEye);
        across=glm::normalize(across);up=glm::cross(toEye,across);
    }
    static glm::mat4 billboard(const glm::vec3& at,const glm::vec3& eye) {
        glm::vec3 x,y,z;facing(at,eye,{1,0,0},x,y,z);
        return {glm::vec4(x,0),glm::vec4(y,0),glm::vec4(z,0),glm::vec4(at,1)};
    }
    static void pictures(Layout& l,int hand,const Words& words) {
        for(int b=0;b<6;b++) {
            auto& p=l.pictures[b];p={};
            if((hand==1&&b==Menu)||words[b].empty())continue;
            int w{},h{};p.texture=vr::xr::textTexture(words[b],40,vr::xr::theme::Text,w,h);
            p.hy=Height*.5f;p.hx=h?p.hy*w/h:0;
        }
    }
    static void layOut(Layout& l,int hand,const glm::mat4& model,const glm::vec3& eye) {
        std::array<glm::vec3,6> at;glm::vec3 middle(0);int points=0;
        for(int b=0;b<6;b++)if(l.pictures[b].texture){at[b]=glm::vec3(model*glm::vec4(Points[hand][b],1));middle+=at[b];++points;}
        if(!points){l.count=0;return;}
        middle/=float(points);glm::vec3 across,up,toEye;facing(middle,eye,glm::vec3(model[0]),across,up,toEye);
        auto x=[&](int b){return glm::dot(at[b]-middle,across);};
        auto y=[&](int b){return glm::dot(at[b]-middle,up);};
        auto room=[&](float px,float py){return middle+across*px+up*py;};
        const float side=hand==1?1.f:-1.f;float outer=0,inner=0;
        for(int b=0;b<6;b++)if(l.pictures[b].texture){outer=std::max(outer,side*x(b));inner=std::max(inner,-side*x(b));}
        const float column=side*(outer+Gap);std::array<float,3> ys{};float buttonsY=0,labelsY=0;
        const int rows=hand==0?3:2;int active=0;float previous=0;
        for(int b=0;b<rows;b++)if(l.pictures[b].texture){buttonsY+=y(b);ys[b]=active?std::min(y(b),previous-Pitch):y(b);previous=ys[b];labelsY+=ys[b];++active;}
        const float shift=active?(buttonsY-labelsY)/active:0;for(int b=0;b<rows;b++)ys[b]+=shift;
        l.count=0;
        auto place=[&](int b,float px,float py,float align,const glm::vec3& to){
            const auto& picture=l.pictures[b];if(!picture.texture)return;
            const float centreX=px+align*picture.hx;
            l.placed[l.count++]={b,room(centreX,py),at[b],to};
        };
        for(int b=0;b<rows;b++)place(b,column,ys[b],side,room(column,ys[b]));
        const float stickX=l.pictures[Stick].texture?x(Stick):0,stickY=std::max(l.pictures[Stick].texture?y(Stick):0,previous)+StickRise;
        place(Stick,stickX,stickY,0,room(stickX,stickY-Height*.5f));
        // Button callouts extend outside each hand. Trigger extends the other
        // way, beside its real anchor rather than above the Stick legend.
        if(l.pictures[Trigger].texture) {
            const float triggerColumn=-side*(inner+TriggerGap);
            place(Trigger,triggerColumn,y(Trigger),-side,room(triggerColumn,y(Trigger)));
        }
        if(l.pictures[Grip].texture)place(Grip,x(Grip),y(Grip)-.045f,0,room(x(Grip),y(Grip)-.045f+Height*.5f));
    }
public:
    // model is exactly the model matrix used to draw this bundled controller.
    // Both eyes share the layout until the controller or viewpoint moves.
    void draw(const glm::mat4& vp,int hand,const glm::mat4& model,const Words& words) {
        auto& l=layouts_[hand];const auto eye=eyeOf(vp);
        const bool wordsChanged=l.words!=words;
        if(wordsChanged)pictures(l,hand,words);
        if(wordsChanged||l.model!=model||l.count==0||glm::length(eye-l.eye)>.1f){l.model=model;l.eye=eye;l.words=words;layOut(l,hand,model,eye);}
        const glm::vec4 colour(vr::xr::theme::Accent,.85f);
        for(size_t i=0;i<l.count;i++) {
            const auto& p=l.placed[i];const auto along=p.to-p.from;const float length=glm::length(along);
            if(length>1e-4f) {
                const auto middle=(p.from+p.to)*.5f,x=along/length;auto y=glm::cross(x,eye-middle);
                if(glm::length(y)>1e-6f){y=glm::normalize(y);glm::mat4 line(glm::vec4(x,0),glm::vec4(y,0),glm::vec4(glm::cross(x,y),0),glm::vec4(middle,1));vr::xr::drawTexture(vp,line,0,length*.5f,LineWidth*.5f,colour);}
            }
            vr::xr::drawTexture(vp,billboard(p.from,eye),0,Dot*.5f,Dot*.5f,colour);
        }
        for(size_t i=0;i<l.count;i++){const auto& p=l.placed[i];const auto& picture=l.pictures[p.button];vr::xr::drawTexture(vp,billboard(p.centre,eye),picture.texture,picture.hx,picture.hy);}
    }
};
}
