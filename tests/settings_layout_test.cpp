#include "../native/shared/xr_panel.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#ifdef GUNBLADE_SETTINGS_RENDER_TEST
#include "../native/gl_context.h"
#include <glad/gl.h>
#include <filesystem>
#include <fstream>
#include <cstdint>
#endif

using namespace vr::xr;
static void check(bool okay, const char* why) {
    if (!okay) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); }
}
static glm::vec2 centre(SettingsRect r) { return {r.x+r.w*.5f,r.y+r.h*.5f}; }
static void expect(SettingsHit hit, SettingsHit::Kind kind, int index=-1, int choice=-1, int direction=0) {
    check(hit.kind==kind && hit.index==index && hit.choice==choice && hit.direction==direction,
          "painted target resolves to its typed action, absolute choice and step direction");
}
#ifdef GUNBLADE_SETTINGS_RENDER_TEST
static void renderChecks(const char* assets,const char* destination) {
    _putenv_s("GUNBLADE_ASSETS",assets);
    gunblade::GLContext context;
    constexpr int width=1200,height=880;
    GLuint image{},fbo{};glGenTextures(1,&image);glBindTexture(GL_TEXTURE_2D,image);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,image,0);
    check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"settings render framebuffer is complete");
    glViewport(0,0,width,height);
    const auto layout=settingsLayout();
    const auto vp=glm::ortho(-layout.width*.5f,layout.width*.5f,-layout.height*.5f,layout.height*.5f,-1.f,1.f);
    auto capture=[&](const SettingsPanel& panel){
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
        drawSettingsPanel(vp,glm::mat4(1),panel);
        std::vector<unsigned char> pixels(width*height*4);
        glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        check(glGetError()==GL_NO_ERROR,"sectioned settings draw has no GL errors");
        return pixels;
    };
    auto save=[&](const char* name,const SettingsPanel& panel){
        auto pixels=capture(panel);check(pixels==capture(panel),"cached settings texture is visually stable");
        std::filesystem::create_directories(destination);
        std::ofstream out(std::filesystem::path(destination)/(std::string(name)+".bmp"),std::ios::binary);
        auto u16=[&](uint16_t n){out.put(char(n));out.put(char(n>>8));};
        auto u32=[&](uint32_t n){u16(n);u16(n>>16);};
        out.write("BM",2);u32(54+width*height*3);u32(0);u32(54);u32(40);u32(width);u32(height);
        u16(1);u16(24);u32(0);u32(width*height*3);u32(0);u32(0);u32(0);u32(0);
        for(size_t i=0;i<pixels.size();i+=4){out.put(char(pixels[i+2]));out.put(char(pixels[i+1]));out.put(char(pixels[i]));}
        check(bool(out),"real GL panel capture is written");
        return pixels;
    };
    auto choices=[](const char* label,std::vector<std::string> values,int choice,const char* note=""){
        SettingsRow row;row.label=label;row.choices=std::move(values);row.choice=choice;row.note=note;return row;
    };
    auto step=[](const char* label,const char* value,const char* note=""){
        SettingsRow row;row.label=label;row.value=value;row.note=note;row.stepper=true;return row;
    };
    auto action=[](const char* label,const char* note=""){
        SettingsRow row;row.label=label;row.note=note;row.action=true;return row;
    };
    SettingsPanel p;p.activeTab=0;p.title="Gun";p.subtitle="Your weapon, aim and feedback.";
    p.rows={choices("Gun model",{"Arcade","Zapper"},0),step("Gun angle","0\xc2\xb0"),
        choices("Laser",{"Off","Normal","2x","5x","10x"},1,"Normal matches Confidential Mission"),choices("Crosshair",{"In-game","Custom","None"},0),
        choices("Haptics",{"Off","Light","Medium","Strong"},2),action("Advanced gun settings")};
    auto gun=save("gun",p);
    p.rows[0].choice=1;check(capture(p)!=gun,"selected absolute choice invalidates cached picture");
    p.rows[0].choice=0;p.rows[0].note="Arcade gun";
    check(capture(p)!=gun,"context notes invalidate cached picture");
    p.title="Advanced gun settings";p.subtitle="Fine-tune gun size and crosshair scaling.";p.back=true;
    p.rows={step("Gun size","100%"),step("Crosshair min size","22%","Far-distance size"),
        step("Crosshair max size","100%","Close-up size"),
        step("Crosshair shrink distance","6.7 m","Distance over which the crosshair shrinks")};
    p.rows[0].dim=true;p.rows[3].selected=true;
    const auto disabled=capture(p);p.rows[0].value="150%";
    check(capture(p)!=disabled,"disabled control still displays its preserved value");
    p.rows[0].value="100%";p.rows[0].dim=false;save("advanced-gun",p);
    p.activeTab=1;p.title="World";p.subtitle="Choose your view. Fine-tuning is in Advanced.";p.back=false;
    p.footer="Trigger selects / Stick adjusts / Menu closes";
    p.rows={choices("Wider view",{"Off","On","Fade"},1,"Fade softens scenery outside the arcade screen"),action("Advanced world settings")};
    save("world",p);
    p.title="Advanced world settings";p.subtitle="Fine-tune world scale, HUD and screen placement.";p.back=true;
    p.rows={step("World size","0.080"),step("HUD size","100%","Applies in Wider view"),
        step("Panel distance","1.8 m"),step("View width","65\xc2\xb0")};
    save("advanced-world",p);
    p.activeTab=2;p.title="System";p.subtitle="Player hands, service and app controls.";p.back=false;
    p.footer="Reset restores VR settings and preserves arcade operator settings.";
    p.rows={choices("Two players",{"P1 left / P2 right","P1 right / P2 left"},0,"Player-hand assignment"),
        action("Open Service Menu"),action("How to play"),action("Reset to defaults","Restore VR settings"),action("Quit game")};
    save("system",p);
    p.activeTab=3;p.title="In-game";p.subtitle="Original arcade game options.";p.footer="Available in attract mode. Native options persist through arcade storage.";
    p.rows={choices("Difficulty",{"1","2","3","4","5","6","7","8"},3),
        choices("Lives",{"1","2","3","4","5","6","7"},4),choices("Attract sound",{"Off","On"},1)};
    save("game",p);
    p.dialog=true;p.activeTab=2;p.title="Reset VR settings?";
    p.footer="Aim + trigger to confirm  |  Menu to close";
    p.subtitle="Restore gun, aiming and world settings to their defaults. Game progress, files and arcade operator settings stay as they are.";
    p.rows={action("Cancel"),action("Reset VR settings")};p.rows[0].selected=true;
    save("reset-confirmation",p);
    p.title="Quit Gunblade NY?";p.subtitle="Return to the headset library.";
    p.rows={action("Cancel"),action("Quit game")};p.rows[0].selected=true;
    save("quit-confirmation",p);
    termPanel();glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&image);
    std::cout<<"Real GL settings pages, texture cache and confirmation captures passed\n";
}
#endif
int main(int argc,char** argv) {
    // Catches old row-count sizing, row/header overlap and accidental activation of disabled rows.
    SettingsPanel panel; panel.title="Gun"; panel.rows.resize(6);
    panel.rows[0].label="Gun model";panel.rows[0].choices={"Arcade","Zapper"};panel.rows[0].choice=0;
    panel.rows[1].label="Gun angle";panel.rows[1].value="0 degrees";panel.rows[1].stepper=true;
    panel.rows[2].label="Laser";panel.rows[2].choices={"Off","On"};panel.rows[2].dim=true;
    panel.rows[3].label="Crosshair";panel.rows[3].choices={"In-game","Custom","None"};
    panel.rows[4].label="Haptics";panel.rows[4].choices={"Off","Light","Medium","Strong"};
    panel.rows[5].label="Advanced gun settings";panel.rows[5].action=true;
    const auto layout=settingsLayout();
    check(layout.width>=1.f && layout.width<=1.2f && layout.height>.7f && layout.height<.9f,
          "six controls keep a stable comfortable metre-scale footprint");
    float lastBottom=.3f;
    for(int i=0;i<6;++i) {
        const auto rect=settingsRowRect(panel,i);
        check(rect.w>.8f && rect.h>.07f && rect.y>lastBottom && rect.y+rect.h<.91f,
              "six large separated hit cards stay between section heading and footer");
        lastBottom=rect.y+rect.h;
        const auto hit=settingsAt(panel,centre(rect));
        if(i==2)expect(hit,SettingsHit::Kind::None);
        else expect(hit,SettingsHit::Kind::Row,i);
        expect(settingsAt(panel,{rect.x+.01f,rect.y-.003f}),SettingsHit::Kind::None);
    }
    check(settingsRowRect(panel,-1).w==0 && settingsRowRect(panel,6).w==0,
          "missing rows expose no actionable rectangle");
    for(int i=0;i<4;++i) expect(settingsAt(panel,centre(settingsTabRect(i))),SettingsHit::Kind::Tab,i);
    expect(settingsAt(panel,centre(settingsCloseRect())),SettingsHit::Kind::Close);
    expect(settingsAt(panel,centre(settingsBackRect())),SettingsHit::Kind::None);
    panel.back=true;
    expect(settingsAt(panel,centre(settingsBackRect())),SettingsHit::Kind::Back);
    expect(settingsAt(panel,{-.01f,.5f}),SettingsHit::Kind::None);
    expect(settingsAt(panel,{1.01f,.5f}),SettingsHit::Kind::None);
    expect(settingsAt(panel,{.5f,.96f}),SettingsHit::Kind::None);
    expect(settingsAt(panel,{std::numeric_limits<float>::quiet_NaN(),.5f}),SettingsHit::Kind::None);

    // Catches the old "any point in the left/right half steps" interaction.
    auto r=settingsRowRect(panel,1);const float y=r.y+r.h*.5f;
    expect(settingsAt(panel,{.75f,y}),SettingsHit::Kind::Row,1,-1,-1);
    expect(settingsAt(panel,{.93f,y}),SettingsHit::Kind::Row,1,-1,1);
    expect(settingsAt(panel,{.85f,y}),SettingsHit::Kind::Row,1);
    expect(settingsAt(panel,{.12f,y}),SettingsHit::Kind::Row,1);
    // Absolute choice positions, including numeric service options, never return +/-1 cycling.
    r=settingsRowRect(panel,0);
    expect(settingsAt(panel,{.83f,r.y+r.h*.5f}),SettingsHit::Kind::Row,0,0);
    expect(settingsAt(panel,{.92f,r.y+r.h*.5f}),SettingsHit::Kind::Row,0,1);
    panel.rows[0].choices={"1","2","3","4","5","6","7","8"};
    int seen=0;
    for(int x=0;x<1200;++x) {
        auto hit=settingsAt(panel,{(x+.5f)/1200.f,r.y+r.h*.5f});
        if(hit.kind==SettingsHit::Kind::Row && hit.choice>=0)seen|=1<<hit.choice;
    }
    check(seen==255,"all eight native difficulty choices have independent direct targets");

    // Ray/world transform agrees with normalized paint coordinates and rejects rear/parallel rays.
    const auto model=glm::translate(glm::mat4(1),glm::vec3(.3f,.2f,-1.5f)) *
                     glm::rotate(glm::mat4(1),.25f,glm::vec3(0,1,0));
    const auto uv=centre(settingsRowRect(panel,5));
    const auto local=glm::vec3((uv.x-.5f)*layout.width,(.5f-uv.y)*layout.height,0);
    const auto target=glm::vec3(model*glm::vec4(local,1));
    const auto origin=glm::vec3(model*glm::vec4(local+glm::vec3(0,0,1),1));
    glm::vec3 point;glm::vec2 got;
    check(settingsPanelHit(model,panel,origin,target-origin,&point,&got),"rotated panel receives front-facing ray");
    check(glm::length(point-target)<1e-5f && glm::length(got-uv)<1e-5f,"pointer and paint UV stay aligned after panel transform");
    expect(settingsHit(model,panel,origin,target-origin),SettingsHit::Kind::Row,5);
    check(!settingsPanelHit(model,panel,target+(target-origin),origin-target),"rear ray misses the panel");
    check(!settingsPanelHit(glm::mat4(1),panel,{0,0,1},{1,0,0}),"parallel ray misses the panel");

    // Confirmation keeps the physical bounds but blocks tabs, Close and Back until an explicit row.
    panel.dialog=true;panel.rows.resize(2);panel.rows[0].dim=false;panel.rows[0].choices.clear();
    panel.rows[0].action=panel.rows[1].action=true;panel.rows[1].stepper=false;
    expect(settingsAt(panel,centre(settingsTabRect(0))),SettingsHit::Kind::None);
    expect(settingsAt(panel,centre(settingsCloseRect())),SettingsHit::Kind::None);
    expect(settingsAt(panel,centre(settingsBackRect())),SettingsHit::Kind::None);
    expect(settingsAt(panel,centre(settingsRowRect(panel,0))),SettingsHit::Kind::Row,0);
    expect(settingsAt(panel,centre(settingsRowRect(panel,1))),SettingsHit::Kind::Row,1);
    check(settingsLayout().height==layout.height,"confirmation retains the main panel footprint");
    panel.dialog=false;panel.locked=true;
    expect(settingsAt(panel,centre(settingsTabRect(0))),SettingsHit::Kind::None);
    expect(settingsAt(panel,centre(settingsCloseRect())),SettingsHit::Kind::None);
    expect(settingsAt(panel,centre(settingsRowRect(panel,0))),SettingsHit::Kind::None);
    std::cout<<"Settings layout and transformed pointer checks passed\n";
#ifdef GUNBLADE_SETTINGS_RENDER_TEST
    if(argc==3)renderChecks(argv[1],argv[2]);
#endif
}
