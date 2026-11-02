// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/gl_scene.h"
#include "../native/gl_context.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace gunblade;
namespace {
constexpr int width=992,height=768,left=248,top=192;
using Image=std::vector<uint8_t>;
void require(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);}
NativeFrame frame(){
    NativeFrame f;f.width=496;f.height=384;f.yoffset=127;
    f.lens={496,384,400,400,248,191};f.complete=true;
    f.background.assign(496*384,0xff000000);f.foreground.assign(496*384,0);
    f.blobs={{"texture0",std::vector<uint8_t>(1048576)},{"texture1",std::vector<uint8_t>(1048576)},
        {"palette",std::vector<uint8_t>(16384)},{"colorxlat",std::vector<uint8_t>(49152)},
        {"luma",std::vector<uint8_t>(32768)},{"gamma",std::vector<uint8_t>(256)}};
    unsigned base=5|(10<<5)|(15<<10);f.blobs[2].bytes[8192]=base;f.blobs[2].bytes[8193]=base>>8;
    for(unsigned c=0;c<3;c++)for(unsigned component=0;component<32;component++)for(unsigned luma=0;luma<64;luma++)
        f.blobs[3].bytes[2*(c*8192+component*256+luma)]=component*4+luma;
    for(unsigned i=0;i<32768;i++)f.blobs[4].bytes[i]=(i&127)/2;
    for(unsigned i=0;i<256;i++)f.blobs[5].bytes[i]=255-i;
    return f;
}
NativePolygon quad(float x0=-248,float y0=-192,float x1=744,float y1=576,float depth=200){
    NativePolygon p;p.count=4;p.focus={400,400};p.center={248,320};p.viewport={0,128,496,512};p.luma=252;
    const float xy[4][2]={{x0,y0},{x1,y0},{x1,y1},{x0,y1}};
    for(unsigned j=0;j<4;j++)p.vertices[j]={(xy[j][0]-248)*depth,(191-xy[j][1])*depth,depth,0,0};
    return p;
}
std::vector<NativePolygon> reticles(){
    // Audited native source/material identity, with synthetic square artwork.
    std::vector<NativePolygon> polygons;
    for(unsigned n=0;n<30;n++){
        auto p=quad(238,181,258,201,10);p.attr=132609;p.window=4;p.focus={600,600};p.luma=255;
        if(n>=28){p.count=3;p.attr=n==28?132354:132610;}
        if(n<3||(n>=19&&n<24)){p.attr=132097;p.culled=true;}
        p.matrix={.08f,0,0,0,.08f,0,0,0,.08f,0,0,10};
        p.object=n<19?0x80cb22:0x80ccac;p.header={192,0,0,n<19?384u:10496u};
        polygons.push_back(p);
    }
    return polygons;
}
std::array<uint8_t,4> pixel(const Image& image,int x,int y){
    const auto at=size_t((height-1-(y+top))*width+x+left)*4;
    return {image[at],image[at+1],image[at+2],image[at+3]};
}
void save(const std::filesystem::path& path,const Image& image){
    std::ofstream output(path,std::ios::binary);output<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(int y=height-1;y>=0;y--)for(int x=0;x<width;x++)output.write(reinterpret_cast<const char*>(image.data()+(y*width+x)*4),3);
    require(bool(output),"cannot write rendered evidence");
}
}
int main(int argc,char** argv){try{
    GLContext context;GLScene scene;GLuint fbo,color,depth;
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"framebuffer incomplete");glViewport(0,0,width,height);
    const float n=.02f;
    const auto projection=glm::frustum(-496.f/400*n,496.f/400*n,-385.f/400*n,383.f/400*n,n,1000.f);
    auto capture=[&](bool fade,const glm::mat4& view=glm::mat4(1)){
        scene.set_view_fade(fade);glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        scene.draw(projection*view,.01f,2.f);Image image(width*height*4);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,image.data());return image;
    };
    auto f=frame();f.polygons={quad()};auto image=std::make_shared<NativeFrame>(f);
    scene.set_immersive(true);scene.prepare(image,true);
    const auto on=capture(false),fade=capture(true);
    require(pixel(on,248,191)==std::array<uint8_t,4>{172,152,132,255},"fixture must render native material color");
    for(int y=0;y<384;y++)for(int x=0;x<496;x++)require(pixel(on,x,y)==pixel(fade,x,y),"Fade changed an original arcade pixel");
    // A missing/disabled fade or a screen-space vignette fails these samples.
    require(pixel(fade,-64,191)[0]<pixel(on,-64,191)[0],"Fade did not darken scenery outside the original left edge");
    for(int dx:{-1,0,1})for(int dy:{-1,0,1})if(dx||dy){
        std::array<uint8_t,4> previous={255,255,255,255};
        for(int offset:{8,24,48,72,96,128}){
            const int x=dx<0?-offset:dx>0?495+offset:248;
            const int y=dy<0?-(offset*3/4):dy>0?383+offset*3/4:191;
            const auto p=pixel(fade,x,y);
            for(unsigned c=0;c<3;c++)require(p[c]<=previous[c],"outside edge/corner brightness increased with distance");
            require(p[3]==255,"fade changed native material alpha");previous=p;
        }
        require(previous==std::array<uint8_t,4>{0,0,0,255},"outside edge/corner did not reach black");
    }
    require(on==capture(false),"switching Fade back to On did not restore all pixels");
    scene.prepare(image,false);require(capture(false)==capture(true),"Original view was altered by saved Fade");
    scene.prepare(image,true);scene.set_immersive(false);
    require(pixel(capture(true),-64,191)[0]<pixel(capture(false),-64,191)[0],"reference reconstructed scenery did not fade");
    scene.set_immersive(true);
    // A translated eye must preserve source-raster gating rather than move the
    // unchanged rectangle with the display. source x=-64 moves to display x=-80.
    const auto lean=glm::translate(glm::mat4(1),glm::vec3(-.08f,0,0));
    const auto leaned_fade=capture(true,lean),leaned_on=capture(false,lean);
    require(pixel(leaned_fade,-80,191)==pixel(fade,-64,191),"fade border followed eye projection instead of source raster");
    for(int y=0;y<384;y++)for(int x=0;x<496;x++)require(pixel(leaned_fade,x-16,y)==pixel(leaned_on,x-16,y),"eye lean changed an original source-raster pixel");
    // Behind the original camera, signed projection would fold back into the
    // central raster. Such geometry must stay black when the user turns around.
    f=frame();f.polygons={quad(148,91,348,291,-200)};scene.prepare(std::make_shared<NativeFrame>(f),true);
    const auto turn=glm::rotate(glm::mat4(1),glm::radians(180.f),glm::vec3(0,1,0));
    require(pixel(capture(false,turn),248,191)[0]>0,"behind-camera signed fixture is not visible with On");
    require(pixel(capture(true,turn),248,191)==std::array<uint8_t,4>{0,0,0,255},"behind-camera scenery folded into bright original raster");
    // A side-facing quad crosses native z=0 while remaining fully visible to
    // the turned HMD. The signed source divide must not create a bright seam.
    auto crossing=quad();crossing.vertices={NativeVertex{80000,40000,100,0,0},NativeVertex{80000,40000,-100,0,0},
        NativeVertex{80000,-40000,-100,0,0},NativeVertex{80000,-40000,100,0,0}};
    f.polygons={crossing};scene.prepare(std::make_shared<NativeFrame>(f),true);
    const auto side=glm::rotate(glm::mat4(1),glm::radians(90.f),glm::vec3(0,1,0));
    require(pixel(capture(false,side),248,191)[0]>0,"camera-plane-crossing fixture is not visible with On");
    const auto crossing_fade=capture(true,side);
    for(int x:{198,248,298})for(int y:{141,191,241})require(pixel(crossing_fade,x,y)==std::array<uint8_t,4>{0,0,0,255},"native camera-plane crossing created a bright fade seam");
    // Native foreground and a known native panel retain their complete pixels.
    f=frame();auto panel=quad(240,180,260,202);panel.focus={1,1};f.polygons={panel};
    f.foreground[191*496+228]=0xff12ab34;scene.prepare(std::make_shared<NativeFrame>(f),true);
    require(capture(false)==capture(true),"native panel/foreground changed with Fade");
    // Relocate verified native artwork beyond the raster's fade-to-black edge.
    f=frame();f.polygons=reticles();scene.prepare(std::make_shared<NativeFrame>(f),true);
    require(scene.reticle_diagnostics().players[0].reason_flags==0,"native reticle fixture did not match production identity");
    scene.set_reticle_mode(ReticleMode::Ingame,true);scene.set_reticle_targets({Vec3{-2.3,0,-2},std::nullopt},glm::mat4(1));
    const auto native_on=capture(false);require(pixel(native_on,-212,191)[0]>0,"native reticle pass did not draw beyond raster");
    require(native_on==capture(true),"relocated original native reticle was faded");
    scene.set_reticle_targets({},glm::mat4(1));
    for(auto& p:f.polygons){p.matrix[9]=-7;for(auto& v:p.vertices)v.x-=4200;}
    scene.prepare(std::make_shared<NativeFrame>(f),true);
    const auto unrelocated=capture(false);require(pixel(unrelocated,-32,191)[0]>0,"unrelocated native reticle fixture did not draw");
    require(unrelocated==capture(true),"unrelocated original native reticle was faded");
    f=frame();f.blobs.clear();f.native.assign(496*384,0xff123456);scene.prepare(std::make_shared<NativeFrame>(f),true);
    const auto fallback=capture(false);require(pixel(fallback,248,191)==std::array<uint8_t,4>{18,52,86,255},"fallback fixture must render native raster");
    require(fallback==capture(true),"native-raster fallback pixels changed with Fade");
    if(argc>1){std::filesystem::create_directories(argv[1]);save(std::filesystem::path(argv[1])/"view-on.ppm",on);save(std::filesystem::path(argv[1])/"view-fade.ppm",fade);}
    require(glGetError()==GL_NO_ERROR,"OpenGL error after production fade draws");
    glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&color);glDeleteRenderbuffers(1,&depth);
    std::cout<<"Fade GL: 190464 identical interior pixels; monotonic 8 edges/corners; black outer border; On restore; Original/reference/lean/signed-behind-camera/camera-plane-crossing; native HUD/reticle/fallback pass\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
