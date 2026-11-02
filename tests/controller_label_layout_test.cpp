// SPDX-License-Identifier: GPL-2.0-or-later
// Exercises the rendered production layout with real font metrics and GLB models.
#define GUNBLADE_CONTROLLER_LABEL_LAYOUT_TEST
#include "../native/controller_labels.h"
#include "../native/presentation.h"
#include "../native/gl_context.h"
#include "../native/shared/glb_mesh.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

namespace gunblade::controller_help {
struct LabelLayoutProbe {
    struct Label {int button;glm::vec3 centre,from,to;float hx,hy;};
    static std::vector<Label> snapshot(const Labels& labels,int hand) {
        const auto& l=labels.layouts_[hand];std::vector<Label> result;
        for(size_t i=0;i<l.count;i++) {
            const auto& p=l.placed[i];const auto& image=l.pictures[p.button];
            result.push_back({p.button,p.centre,p.from,p.to,image.hx,image.hy});
        }
        return result;
    }
};
}
using namespace gunblade;
using Label=controller_help::LabelLayoutProbe::Label;
static int failures{};
static void check(bool okay,const std::string& why) {
    if(!okay){std::cerr<<"FAIL: "<<why<<'\n';++failures;}
}
static glm::vec2 screen(const glm::mat4& vp,glm::vec3 at) {
    auto p=vp*glm::vec4(at,1);return glm::vec2(p)/p.w;
}
struct Rect {glm::vec2 lo{std::numeric_limits<float>::max()},hi{std::numeric_limits<float>::lowest()};};
static void include(Rect& r,glm::vec2 p){r.lo=glm::min(r.lo,p);r.hi=glm::max(r.hi,p);}
static bool overlaps(const Rect& a,const Rect& b,float gap=0) {
    return a.lo.x<b.hi.x+gap&&a.hi.x>b.lo.x-gap&&a.lo.y<b.hi.y+gap&&a.hi.y>b.lo.y-gap;
}
static bool triangleHits(const Rect& r,glm::vec2 a,glm::vec2 b,glm::vec2 c) {
    // Test the actual projected silhouette, not a whole-hand bounding box:
    // a rolled grip can extend inward well below the horizontal Trigger plate.
    const glm::vec2 edges[]={b-a,c-b,a-c};
    for(int i=0;i<5;i++) {
        const auto axis=i==0?glm::vec2(1,0):i==1?glm::vec2(0,1):glm::vec2(-edges[i-2].y,edges[i-2].x);
        if(glm::dot(axis,axis)<1e-12f)continue;
        const float lo=std::min({glm::dot(a,axis),glm::dot(b,axis),glm::dot(c,axis)}),hi=std::max({glm::dot(a,axis),glm::dot(b,axis),glm::dot(c,axis)});
        const auto middle=(r.lo+r.hi)*.5f,extent=(r.hi-r.lo)*.5f;
        const float at=glm::dot(middle,axis),radius=glm::dot(extent,glm::abs(axis));
        if(hi<at-radius||lo>at+radius)return false;
    }
    return true;
}
static Rect plate(const Label& l,const glm::mat4& vp,glm::vec3 eye) {
    auto z=glm::normalize(eye-l.centre),x=glm::normalize(glm::cross(glm::vec3(0,1,0),z)),y=glm::cross(z,x);
    Rect r;for(float sx:{-1.f,1.f})for(float sy:{-1.f,1.f})include(r,screen(vp,l.centre+x*sx*l.hx+y*sy*l.hy));
    return r;
}
static void save(const std::filesystem::path& path,int width,int height) {
    std::vector<unsigned char> pixels(size_t(width)*height*4);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    std::ofstream out(path,std::ios::binary);
    auto u16=[&](uint16_t n){out.put(char(n));out.put(char(n>>8));};auto u32=[&](uint32_t n){u16(n);u16(n>>16);};
    out.write("BM",2);u32(54+width*height*3);u32(0);u32(54);u32(40);u32(width);u32(height);u16(1);u16(24);u32(0);u32(width*height*3);u32(0);u32(0);u32(0);u32(0);
    for(size_t i=0;i<pixels.size();i+=4){out.put(char(pixels[i+2]));out.put(char(pixels[i+1]));out.put(char(pixels[i]));}
    check(bool(out),"actual GL capture saved: "+path.string());
}
int main(int argc,char** argv) {
    if(argc!=3)return 2;_putenv_s("GUNBLADE_ASSETS",argv[1]);GLContext context;
    const auto output=std::filesystem::path(argv[2]);std::filesystem::create_directories(output);
    constexpr int width=1600,height=1000;GLuint image{},depth{},fbo{};
    glGenTextures(1,&image);glBindTexture(GL_TEXTURE_2D,image);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,image,0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);glViewport(0,0,width,height);
    check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"complete capture framebuffer");
    vr::xr::GlbMesh meshes[2];
    // Independent asset-sidecar fixtures; catches anchoring a callout to Stick or a mirrored hand.
    const glm::vec3 trigger[2]={{-.0142f,-.0159f,.0254f},{.0142f,-.0159f,.0254f}};
    for(int hand=0;hand<2;hand++) {
        std::ifstream in(std::filesystem::path(argv[1])/(hand?"models/controllers/right.glb":"models/controllers/left.glb"),std::ios::binary);
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};std::string error;
        check(!bytes.empty()&&vr::xr::loadGlb(bytes.data(),bytes.size(),meshes[hand],error,8,1024),"load actual bundled controller");
        float nearest=100;
        for(size_t i=0;i<meshes[hand].positions.size();i+=3)nearest=std::min(nearest,glm::length(trigger[hand]-glm::vec3(meshes[hand].positions[i],meshes[hand].positions[i+1],meshes[hand].positions[i+2])));
        check(nearest<.008f,"documented trigger anchor lies on real bundled controller surface");
    }
    struct Pose {const char* name;float pitch,yaw,roll,scale;};
    const Pose poses[]={{"neutral",0,0,0,1},{"tilted",-25,0,18,1},{"yawed",-15,35,-12,1},{"small",-15,-25,8,.8f},{"large",-20,25,-10,1.3f}};
    const auto projection=glm::perspective(glm::radians(55.f),float(width)/height,.05f,10.f);
    std::ofstream geometry(output/"geometry.csv");
    geometry<<"pose,spacing,legend,eye,hand,button,cx,cy,cz,fromx,fromy,fromz,tox,toy,toz,hx,hy\n";
    Presentation captureUi; // retain the shared panel-texture cache between stereo draws
    for(const auto& pose:poses)for(float spacing:{.17f,.20f})for(int legend=0;legend<3;legend++) {
      controller_help::Labels labels; // both stereo eyes use production's shared cached layout
      for(float eyeX:{-.032f,.032f}) {
        const auto eye=glm::vec3(eyeX,.04f,0);const auto vp=projection*glm::lookAt(eye,eye+glm::vec3(0,0,-1),{0,1,0});
        std::array<std::vector<Label>,2> placed;
        std::array<glm::mat4,2> models;
        for(int hand=0;hand<2;hand++) {
            auto model=glm::translate(glm::mat4(1),glm::vec3(hand?spacing:-spacing,-.10f,-.65f));
            model=glm::rotate(model,glm::radians(pose.yaw*(hand?1:-1)),{0,1,0});
            model=glm::rotate(model,glm::radians(pose.pitch),{1,0,0});model=glm::rotate(model,glm::radians(pose.roll*(hand?1:-1)),{0,0,1});
            model=glm::scale(model,glm::vec3(pose.scale));models[hand]=glm::rotate(model,glm::pi<float>(),{0,1,0});
            Settings settings;GameState game;game.valid=game.ready=true;
            auto words=controller_help::words(hand,settings,game,legend==1,legend==2);
            labels.draw(vp,hand,models[hand],words);placed[hand]=controller_help::LabelLayoutProbe::snapshot(labels,hand);
            for(const auto& l:placed[hand])geometry<<pose.name<<','<<spacing<<','<<legend<<','<<eyeX<<','<<hand<<','<<l.button<<','<<l.centre.x<<','<<l.centre.y<<','<<l.centre.z<<','<<l.from.x<<','<<l.from.y<<','<<l.from.z<<','<<l.to.x<<','<<l.to.y<<','<<l.to.z<<','<<l.hx<<','<<l.hy<<'\n';
            const std::string tag=std::string(pose.name)+" legend="+std::to_string(legend)+" hand="+std::to_string(hand)+" eye="+std::to_string(eyeX);
            const auto find=[&](int button)->const Label&{for(const auto& l:placed[hand])if(l.button==button)return l;std::cerr<<"Missing label "<<button<<'\n';std::exit(2);};
            const auto& fire=find(3);const auto& start=find(1);const auto& stick=find(4);
            const float side=hand?1.f:-1.f;
            check(glm::length(fire.from-glm::vec3(models[hand]*glm::vec4(trigger[hand],1)))<1e-6f,tag+" leader starts at physical trigger");
            check(side*(screen(vp,fire.centre).x-screen(vp,fire.from).x)<-.035f,tag+" Trigger extends inward laterally");
            check(side*(screen(vp,start.centre).x-screen(vp,start.from).x)>.035f,tag+" A/X remains outside");
            const auto fireRect=plate(fire,vp,eye);
            for(const auto& other:placed[hand])if(other.button!=3)check(!overlaps(fireRect,plate(other,vp,eye),.002f),tag+" Trigger plate clears label "+std::to_string(other.button));
            std::vector<glm::vec2> body;const auto& mesh=meshes[hand];
            for(size_t i=0;i<mesh.positions.size();i+=3)body.push_back(screen(vp,glm::vec3(models[hand]*glm::vec4(mesh.positions[i],mesh.positions[i+1],mesh.positions[i+2],1))));
            bool coversController=false;
            for(size_t i=0;i<mesh.indices.size();i+=3)if(triangleHits(fireRect,body[mesh.indices[i]],body[mesh.indices[i+1]],body[mesh.indices[i+2]])){coversController=true;break;}
            check(!coversController,tag+" inward Trigger plate clears actual controller silhouette");
            const auto target=screen(vp,fire.to);const float nearest=hand?fireRect.hi.x:fireRect.lo.x;
            check(std::abs(target.x-nearest)<.005f,tag+" leader meets nearest plate edge");
            check(screen(vp,fire.centre).y<screen(vp,stick.centre).y-.02f,tag+" Trigger is below Stick legend");
        }
        for(const auto& a:placed[0])for(const auto& b:placed[1])check(!overlaps(plate(a,vp,eye),plate(b,vp,eye)),std::string(pose.name)+" separate hands' callouts remain disjoint");
        if(eyeX<0&&legend==0&&spacing==.17f) {
            // Full production Presentation draws the original GLBs, materials and callouts.
            Settings settings;SharedRuntime runtime;PresentationInput input;GameState game;game.valid=game.ready=true;
            for(int hand=0;hand<2;hand++){input.hands[hand].tracked=true;input.poses[hand]=models[hand]*glm::rotate(glm::mat4(1),glm::pi<float>(),{0,1,0});}
            captureUi.update(input,settings,runtime,.2);glDepthMask(GL_TRUE);glClearColor(.035f,.045f,.06f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            captureUi.draw(vp,settings,game,true,.2);save(output/(std::string(pose.name)+".bmp"),width,height);
        }
      }
    }
    {
        controller_help::Labels labels;Settings settings;GameState game;
        const auto vp=projection;const auto model=glm::translate(glm::mat4(1),glm::vec3(-.17f,-.1f,-.65f));
        auto words=controller_help::words(0,settings,game);labels.draw(vp,0,model,words);
        words[3].clear();labels.draw(vp,0,model,words);
        for(const auto& l:controller_help::LabelLayoutProbe::snapshot(labels,0))check(l.button!=3,"clearing a Trigger legend removes its plate and leader");
        labels.draw(vp,0,model,{});check(controller_help::LabelLayoutProbe::snapshot(labels,0).empty(),"empty help leaves no stale callouts");
    }
    check(glGetError()==GL_NO_ERROR,"production controller help renders without GL errors");
    vr::xr::termPanel();glDeleteFramebuffers(1,&fbo);glDeleteRenderbuffers(1,&depth);glDeleteTextures(1,&image);
    if(failures){std::cerr<<failures<<" controller layout checks failed\n";return 1;}
    std::cout<<"Controller callout geometry: real anchors, inward plates, model/label/hand separation; 120 hand layouts (60 stereo-eye/legend/pose/spacing cases) passed\n";
}
