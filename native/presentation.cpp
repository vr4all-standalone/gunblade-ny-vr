#include "laser_settings.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "presentation.h"
#include "settings_io.h"
#include "button_latch.h"
#include "controller_controls.h"
#include "controller_labels.h"
#include "gun_effects.h"
#include "gun_pose.h"
#include "gun_selection.h"
#include "menu_model.h"
#include "menu_pointer.h"
#include "game_option_edits.h"
#include "model2_scene.h"
#include "shared/xr_panel.h"
#include "shared/glb_mesh.h"
#include "shared/gun_model.h"
#include "shared/ui_compat.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace gunblade {
namespace {
std::string number(float n,int digits=2){std::ostringstream o;o<<std::fixed<<std::setprecision(digits)<<n;return o.str();}
void save(const Settings& s) {const char* name=std::getenv("GUNBLADE_SETTINGS");std::ofstream out(name?name:"gunblade-settings.ini");if(out)write_settings(out,s);}
struct Model {
    GLuint vao{},vertices{},indices{};std::vector<GLuint> textures;vr::xr::GlbMesh mesh;glm::mat4 placement{1};glm::vec3 muzzle{0,.0471,-.1401};bool arcade_model{};
    void clear(){for(auto t:textures)glDeleteTextures(1,&t);textures.clear();if(indices)glDeleteBuffers(1,&indices);if(vertices)glDeleteBuffers(1,&vertices);if(vao)glDeleteVertexArrays(1,&vao);vao=vertices=indices=0;mesh={};}
    ~Model(){clear();}
    void upload() {
        std::vector<float> data;data.reserve(mesh.positions.size()*4);
        for(size_t i=0;i<mesh.positions.size()/3;i++) {
            for(int c=0;c<3;c++)data.push_back(mesh.positions[i*3+c]);
            for(int c=0;c<2;c++)data.push_back(mesh.uvs.empty()?0:mesh.uvs[i*2+c]);
            for(int c=0;c<3;c++)data.push_back(mesh.normals[i*3+c]);
            for(int c=0;c<4;c++)data.push_back(mesh.colors.empty()?1:mesh.colors[i*4+c]);
        }
        glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vertices);glBindBuffer(GL_ARRAY_BUFFER,vertices);glBufferData(GL_ARRAY_BUFFER,data.size()*4,data.data(),GL_STATIC_DRAW);
        for(int i=0;i<4;i++){glEnableVertexAttribArray(i);glVertexAttribPointer(i,i==0||i==2?3:i==1?2:4,GL_FLOAT,GL_FALSE,48,reinterpret_cast<void*>(uintptr_t(i==0?0:i==1?12:i==2?20:32)));}
        glGenBuffers(1,&indices);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indices);glBufferData(GL_ELEMENT_ARRAY_BUFFER,mesh.indices.size()*4,mesh.indices.data(),GL_STATIC_DRAW);
        for(const auto& im:mesh.images) {GLuint texture{};glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,im.width,im.height,0,GL_RGBA,GL_UNSIGNED_BYTE,im.rgba.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);textures.push_back(texture);}
    }
    void arcade() {
        using namespace vr::xr::gunmodel;
        clear();arcade_model=true;for(auto p:Positions)mesh.positions.push_back(p*PositionScale);for(auto n:Normals)mesh.normals.push_back(n/127.f);
        for(auto i:Indices)mesh.indices.push_back(i);
        const float colors[][4]={{.5,.035,.05,1},{.035,.035,.045,1},{.45,.45,.47,1},{.12,.22,.3,1}};
        for(unsigned i=0;i<std::size(Parts);i++){vr::xr::GlbPart part;part.firstIndex=Parts[i].first;part.indexCount=Parts[i].count;std::copy_n(colors[i],4,part.color);mesh.parts.push_back(part);}
        placement=glm::translate(glm::mat4(1),glm::vec3(0,-.06,.006));muzzle={0,.0471,-.1401};upload();
    }
    bool file(const char* name,bool zapper) {
        size_t size{};auto bytes=resource::load(name,size);if(!bytes||size>16*1024*1024)return false;
        vr::xr::GlbMesh loaded;std::string error;if(!vr::xr::loadGlb(bytes.get(),size,loaded,error,8,1024))return false;
        clear();arcade_model=false;mesh=std::move(loaded);
        if(zapper) {
            glm::vec3 grip(.0281f,.038f,-.105f);muzzle={.02765f,.06172f,.0821f};
            placement=glm::rotate(glm::mat4(1),glm::pi<float>(),glm::vec3(0,1,0))*glm::translate(glm::mat4(1),-grip);
        } else {
            glm::vec3 lo(1e9f),hi(-1e9f);for(size_t i=0;i<mesh.positions.size();i+=3){glm::vec3 p(mesh.positions[i],mesh.positions[i+1],mesh.positions[i+2]);lo=glm::min(lo,p);hi=glm::max(hi,p);}
            float span=glm::max(glm::max(hi.x-lo.x,hi.y-lo.y),hi.z-lo.z);if(!std::isfinite(span)||span<1e-6f){clear();return false;}
            glm::vec3 grip((lo.x+hi.x)*.5f,lo.y+(hi.y-lo.y)*.25f,hi.z-(hi.z-lo.z)*.2f);
            placement=glm::scale(glm::mat4(1),glm::vec3(.25f/span))*glm::translate(glm::mat4(1),-grip);muzzle={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f,lo.z};
        }
        upload();return true;
    }

};
}
struct Presentation::Impl {
    ControllerHelpPolicy startup_help;
    controller_help::Labels controller_labels;
    bool opened{},help{},fresh{},recenter{},quit_armed{},service_mode{},service_seen_native{},service_guns{},native_calibration{},calibration_busy{};double quit_at{};int selected{},loaded_gun{-1};
    SharedRuntime* runtime{};
    MenuPage page{MenuPage::Gun};MenuPointer pointer;
    enum class Confirm {None,Reset,Quit};Confirm confirm{};
    GameOptions options,edit_options;GameOptionEdits game_edits;GameState native_state;bool game_dirty{},game_busy{};GameOptionsStatus game_status{};GameOptionsError game_error{};unsigned game_count{},game_index{};
    void trace(const char* event,const vr::xr::SettingsHit& hit={},bool hand_changed=false) const{
        const char* path=std::getenv("GUNBLADE_XR_LOG");if(!path||!*path)return;
        std::ofstream out(std::string(path)+".menu",std::ios::app);
        out<<event<<" page="<<int(page)<<" owner="<<pointer.owner()<<" changed="<<hand_changed<<" fresh="<<fresh
           <<" focused="<<input.focused<<" hit="<<int(hit.kind)<<'/'<<hit.index<<'/'<<hit.choice<<'/'<<hit.direction
           <<" native="<<native_state.valid<<'/'<<native_state.mode<<'/'<<native_state.submode
           <<" options="<<options.valid<<'/'<<options.eligible<<" dirty="<<game_dirty<<" busy="<<game_busy
           <<" staged="<<edit_options.difficulty<<'/'<<edit_options.shifting_difficulty<<'/'<<edit_options.player_life<<'/'<<edit_options.attract_sound<<'\n';
    }
    std::vector<MenuAction> actions() const {
        if(confirm!=Confirm::None)return {MenuAction::Cancel,MenuAction::Confirm};
        return menu_actions(page,{MenuAction::GameDifficulty,MenuAction::GameShifting,MenuAction::GameLives,MenuAction::GameAttractSound,MenuAction::ApplyGameSettings});
    }
    std::vector<MenuAction> navigation() const {
        if(confirm!=Confirm::None)return actions();
        std::vector<MenuAction> out{MenuAction::Close};out.insert(out.end(),MenuTabs.begin(),MenuTabs.end());
        if(main_page(page)!=page)out.push_back(MenuAction::Back);
        auto rows=actions();out.insert(out.end(),rows.begin(),rows.end());return out;
    }
    MenuAction selected_action() const {auto n=navigation();return n[std::clamp(selected,0,int(n.size())-1)];}
    void focus(MenuAction action){auto n=navigation();auto at=std::find(n.begin(),n.end(),action);if(at!=n.end())selected=int(at-n.begin());}
    void switch_page(MenuPage next){page=next;confirm=Confirm::None;fresh=true;stick_wait=true;hovered[0]=hovered[1]=-1;focus(actions().front());}

    std::array<ButtonLatch,2> trigger_latch,start_latch;ButtonLatch toggle_latch;bool stick_wait{true};glm::vec2 stick_was{};double repeat_at{};
    int hovered[2]{-1,-1};
    glm::mat4 panel_model{1};PresentationInput input;CabinetInput cabinet;std::array<double,2> shot_time{-100,-100};
    Model gun;std::array<Model,2> controllers;bool controllers_loaded{};GLuint model_program{};GunEffects gun_effects;
    vr::xr::SettingsPanel panel(const Settings& s) const {
        using A=MenuAction;vr::xr::SettingsPanel p;
        p.locked=game_busy;p.activeTab=menu_tab_index(page);p.back=main_page(page)!=page;p.game="Gunblade NY";
        auto selected_action=this->selected_action();p.closeSelected=selected_action==A::Close;p.backSelected=selected_action==A::Back;
        for(int t=0;t<4;t++)if(selected_action==MenuTabs[t])p.selectedTab=t;
        const char* titles[]={"Gun","World","System","In-game","Advanced gun settings","Advanced world settings"};p.title=titles[int(page)];
        const char* descriptions[]={"Your weapon, aim and feedback.","Choose your view. Fine-tuning is in Advanced.","Player hands, service and app controls.","Original arcade options. Applying changes restarts the game.","Fine-tune gun size and crosshair scaling.","Fine-tune world scale, HUD and screen placement."};p.subtitle=descriptions[int(page)];
        p.footer="Trigger selects / Stick adjusts / Menu closes";
        if(page==MenuPage::Game){
            p.footer=!options.valid?"Waiting for the original game's settings...":"Apply settings and restart.";
            if(game_status==GameOptionsStatus::Complete&&!game_dirty)p.footer="Settings applied. Game restarted.";
            if(game_status==GameOptionsStatus::Failed)p.footer="Save interrupted. Open Service Menu to finish or exit.";
        }
        if(game_busy)p.footer=game_status==GameOptionsStatus::Restarting?"Applying settings and restarting. Keep the headset focused.":"Saving game settings "+std::to_string(game_index+1)+" / "+std::to_string(game_count)+". Keep the headset focused.";
        if(confirm!=Confirm::None){
            p.dialog=true;p.back=false;p.title=confirm==Confirm::Reset?"Reset VR settings?":"Quit Gunblade NY?";
            p.subtitle=confirm==Confirm::Reset?"Restore gun, aiming and world defaults. Game data and arcade settings are kept.":"Return to your headset library?";
            p.rows.push_back({"Cancel","","",{},-1,selected_action==A::Cancel,false,false,true});
            p.rows.push_back({confirm==Confirm::Reset?"Reset VR settings":"Quit game","","",{},-1,selected_action==A::Confirm,false,false,true});return p;
        }
        for(auto a:actions()){
            vr::xr::SettingsRow r;r.label=menu_label(a);r.selected=selected_action==a;r.dim=game_busy;
            switch(a){
            case A::GunModel:r.choices={"Arcade","Zapper"};r.choice=normalize_gun(s.gun);break;
            case A::GunAngle:r.value=number(s.gun_angle,0)+" deg";r.stepper=true;break;
            case A::Laser:r.choices={"Off","Normal","5x","10x"};r.choice=laser_menu_choice(s.laser,s.laser_thickness);break;
            case A::Reticle:r.choices={"In-game","Custom","None"};r.choice=int(s.reticle_mode);break;
            case A::Haptics:r.choices={"Off","Light","Medium","Strong"};r.choice=haptic_level(s.haptics);break;
            case A::OpenGunAdvanced:case A::OpenWorldAdvanced:case A::OpenService:case A::HowToPlay:case A::ResetDefaults:case A::Quit:r.action=true;break;
            case A::GunSize:r.value=number(s.gun_scale*100,0)+"%";r.stepper=true;break;
            case A::ReticleMinimum:r.value=number((s.reticle_mode==ReticleMode::Custom?s.reticle_custom_min_size:s.reticle_min_size)*100,0)+"%";r.note="Far-distance size";r.stepper=true;r.dim|=s.reticle_mode==ReticleMode::None;break;
            case A::ReticleMaximum:r.value=number((s.reticle_mode==ReticleMode::Custom?s.reticle_custom_max_size:s.reticle_max_size)*100,0)+"%";r.note="Close-up size";r.stepper=true;r.dim|=s.reticle_mode==ReticleMode::None;break;
            case A::ReticleDistance:r.value=number(s.reticle_shrink_distance,1)+" m";r.stepper=true;r.dim|=s.reticle_mode==ReticleMode::None;break;
            case A::WiderView:r.choices={"Off","On","Fade"};r.choice=!s.wider_view?0:s.wider_view_fade?2:1;r.note="Fade softens scenery outside the arcade screen";break;
            case A::WorldScale:r.value=number(s.world_scale,3);r.stepper=true;break;
            case A::HudSize:r.value=number(s.hud_size*100,0)+"%";r.note="Applies in Wider view";r.stepper=true;break;
            case A::PanelDistance:r.value=number(s.panel_distance,1)+" m";r.stepper=true;break;
            case A::ViewWidth:r.value=number(s.view_fov,0)+" deg";r.stepper=true;break;
            case A::TwoPlayers:r.value=s.p1_hand==0?"P1 left / P2 right":"P1 right / P2 left";r.note="Player-hand assignment";r.stepper=true;break;
            case A::GameDifficulty:case A::GameShifting:case A::GameLives:case A::GameAttractSound:{
                r.dim|=!options.valid;r.stepper=a!=A::GameAttractSound;
                if(a==A::GameAttractSound){r.choices={"Off","On"};r.choice=options.valid?int(edit_options.attract_sound):-1;}
                else r.value=!options.valid?"...":a==A::GameDifficulty?std::to_string(edit_options.difficulty+1)+" / 8":a==A::GameShifting?std::to_string(edit_options.shifting_difficulty+1)+" / 8":std::to_string(edit_options.player_life);
                if(a==A::GameLives)r.note="Starting lives";break;
            }
            case A::ApplyGameSettings:r.action=true;r.dim|=!game_dirty||!options.valid||!native_state.valid||native_state.service;if(game_dirty)r.note=native_state.service?"Finish or exit Service first":"Ends the current round";break;
            default:break;
            }
            p.rows.push_back(std::move(r));
        }
        return p;
    }
    void choose(MenuAction action,int direction,int choice,Settings& s,SharedRuntime& runtime,double /*now*/){
        using A=MenuAction;if(game_busy)return;
        if(std::find(MenuTabs.begin(),MenuTabs.end(),action)!=MenuTabs.end()){switch_page(menu_tab_page(action));return;}
        switch(action){
        case A::Close:opened=false;help=false;runtime.release_inputs();pointer.suspend();return;
        case A::Back:switch_page(main_page(page));return;
        case A::OpenGunAdvanced:switch_page(MenuPage::GunAdvanced);return;
        case A::OpenWorldAdvanced:switch_page(MenuPage::WorldAdvanced);return;
        case A::HowToPlay:help=!help;fresh=true;runtime.release_inputs();return;
        case A::OpenService:runtime.release_inputs();if(!service_mode){runtime.service_requested=true;service_seen_native=false;service_guns=false;}else if(native_calibration)service_guns=!service_guns;service_mode=true;opened=false;help=false;pointer.suspend();return;
        case A::ResetDefaults:confirm=Confirm::Reset;selected=0;fresh=true;stick_wait=true;return;
        case A::Quit:confirm=Confirm::Quit;selected=0;fresh=true;stick_wait=true;return;
        case A::Cancel:confirm=Confirm::None;focus(A::ResetDefaults);fresh=true;stick_wait=true;return;
        case A::Confirm:if(confirm==Confirm::Reset){s=Settings{};runtime.release_inputs();help=false;save(s);}else if(confirm==Confirm::Quit)runtime.quit_requested=true;confirm=Confirm::None;focus(A::ResetDefaults);fresh=true;stick_wait=true;return;
        case A::ApplyGameSettings:if(game_dirty&&runtime.begin_game_options_restart(edit_options))game_busy=true;return;
        default:break;
        }
        if(action>=A::GameDifficulty&&action<=A::GameAttractSound){
            if(!options.valid)return;
            auto adjust=[&](uint32_t& value,int lo,int hi){value=uint32_t(std::clamp(int(value)+direction,lo,hi));};
            if(action==A::GameDifficulty)adjust(edit_options.difficulty,0,7);
            if(action==A::GameShifting)adjust(edit_options.shifting_difficulty,0,7);
            if(action==A::GameLives)adjust(edit_options.player_life,1,7);
            if(action==A::GameAttractSound)edit_options.attract_sound=choice>=0?uint32_t(choice):1-edit_options.attract_sound;
            auto option=action==A::GameDifficulty?GameOption::Difficulty:action==A::GameShifting?GameOption::ShiftingDifficulty:action==A::GameLives?GameOption::PlayerLife:GameOption::AttractSound;
            game_edits.stage(options,option,edit_options.value(option));edit_options=game_edits.merge(options);game_dirty=game_edits.dirty(options);return;
        }
        if(!direction&&choice<0)return;
        switch(action){
        case A::GunModel:s.gun=choice>=0?choice:next_gun(s.gun,direction);break;
        case A::GunAngle:s.gun_angle=std::clamp(s.gun_angle+direction,-30.f,30.f);break;
        case A::Laser:{int current=laser_menu_choice(s.laser,s.laser_thickness);int next=std::clamp(choice>=0?choice:current+direction,0,3);s.laser=next!=0;if(s.laser)s.laser_thickness=laser_stored_choice(next);break;}
        case A::Reticle:set_crosshair_mode(s,choice>=0?reticle_mode_value(choice):next_reticle_mode(s.reticle_mode,direction));break;
        case A::Haptics:s.haptics=HapticLevels[choice>=0?choice:std::clamp(haptic_level(s.haptics)+direction,0,3)];break;
        case A::GunSize:s.gun_scale=std::clamp(s.gun_scale+direction*.05f,.4f,1.6f);break;
        case A::ReticleMinimum:if(s.reticle_mode!=ReticleMode::None){auto& value=s.reticle_mode==ReticleMode::Custom?s.reticle_custom_min_size:s.reticle_min_size;value=std::clamp(std::round(value*100)+direction,5.f,100.f)/100;}break;
        case A::ReticleMaximum:if(s.reticle_mode!=ReticleMode::None){auto& value=s.reticle_mode==ReticleMode::Custom?s.reticle_custom_max_size:s.reticle_max_size;value=std::clamp(std::round(value*100)+direction*5.f,25.f,300.f)/100;}break;
        case A::ReticleDistance:if(s.reticle_mode!=ReticleMode::None)s.reticle_shrink_distance=std::clamp(std::round(s.reticle_shrink_distance*10)+direction,2.f,200.f)/10;break;
        case A::WiderView:{int current=!s.wider_view?0:s.wider_view_fade?2:1;int next=choice>=0?choice:std::clamp(current+direction,0,2);s.wider_view=next!=0;s.wider_view_fade=next==2;break;}
        case A::WorldScale:set_custom_world_scale(s,std::clamp(s.world_scale+direction*.005f,.01f,1.f),Settings{}.world_scale);break;
        case A::HudSize:s.hud_size=std::clamp(std::round(s.hud_size*100)+direction*5.f,50.f,200.f)/100;break;
        case A::PanelDistance:s.panel_distance=std::clamp(s.panel_distance+direction*.1f,.5f,10.f);break;
        case A::ViewWidth:s.view_fov=std::clamp(s.view_fov+direction*5.f,35.f,95.f);break;
        case A::TwoPlayers:s.p1_hand=1-s.p1_hand;runtime.release_inputs();fresh=true;stick_wait=true;break;
        default:return;
        }
        save(s);
    }
    void init_model() {
        if(model_program)return;
        OpenGlSource vs,fs;
        vs.addSource(R"(
in vec3 in_pos;in vec2 in_uv;in vec3 in_normal;in vec4 in_color;
uniform mat4 mvp;uniform mat4 model;out vec2 uv;out vec3 normal;out vec4 color;
void main(){gl_Position=mvp*vec4(in_pos,1);uv=in_uv;normal=mat3(model)*in_normal;color=in_color;})");
        fs.addSource(R"(
in vec2 uv;in vec3 normal;in vec4 color;uniform vec4 base;uniform sampler2D tex;uniform bool textured;out vec4 result;
void main(){vec4 c=base*color;if(textured)c*=texture(tex,uv);if(c.a<.1)discard;float light=.35+.65*max(0.,dot(normalize(normal),normalize(vec3(-.4,.8,.5))));result=vec4(c.rgb*light,c.a);})");
        model_program=gl_CompileAndLink(vs.generate().c_str(),fs.generate().c_str());
    }
};
Presentation::Presentation():impl_(std::make_unique<Impl>()){}
Presentation::~Presentation(){if(impl_->model_program)glDeleteProgram(impl_->model_program);vr::xr::termPanel();}
bool Presentation::open() const{return impl_->opened;}
MenuPage Presentation::menu_page() const{return impl_->page;}
MenuAction Presentation::selected_action() const{return impl_->selected_action();}
int Presentation::menu_pointer_hand() const{return impl_->pointer.owner();}
bool Presentation::service() const{return impl_->service_mode;}
bool Presentation::service_guns() const{return impl_->service_guns;}
void Presentation::toggle_service_guns(){if(impl_->native_calibration)impl_->service_guns=!impl_->service_guns;}
void Presentation::prepare(const Settings& s,SharedRuntime* runtime) {
    auto& p=*impl_;if(runtime)p.runtime=runtime;
    if(p.loaded_gun!=s.gun) {
        p.loaded_gun=s.gun;
        if(normalize_gun(s.gun)==0||!p.gun.file("models/guns/zapper.glb",true))p.gun.arcade();
    }
}
void Presentation::suspend(){auto& p=*impl_;p.pointer.suspend();for(auto& latch:p.trigger_latch)latch.suspend();for(auto& latch:p.start_latch)latch.suspend();p.toggle_latch.suspend();p.stick_wait=true;p.input.hands={};p.input.aim_targets={};p.gun_effects.clear();p.shot_time={-100,-100};}
bool Presentation::recenter_requested(){return std::exchange(impl_->recenter,false);}
glm::vec3 Presentation::muzzle(const glm::mat4& aim,const Settings& s) const{return glm::vec3(aim*glm::scale(glm::mat4(1),glm::vec3(s.gun_scale))*impl_->gun.placement*glm::vec4(impl_->gun.muzzle,1));}
void Presentation::feedback(const CabinetInput& in,double now){
    auto& p=*impl_;p.cabinet=in;p.startup_help.observe(in);
    // Native acknowledgements can arrive after update() has already lost tracking/focus.
    for(int player=0;player<2;player++){
        int hand=in.hand[player];
        if(in.accepted_shots[player]&&p.input.focused&&!p.opened&&!p.service_mode&&hand>=0&&hand<2&&p.input.hands[hand].tracked)p.shot_time[player]=now;
    }
}
std::array<std::optional<Vec3>,2> Presentation::reticle_targets(const GameState& state) const {
    const auto& p=*impl_;std::array<std::optional<Vec3>,2> targets;
    if(!reticles_active(state))return targets;
    for(int player=0;player<2;player++){
        int hand=p.cabinet.hand[player];
        if(state.players[player].alive()&&hand>=0&&hand<2&&p.input.hands[hand].tracked)targets[player]=p.input.aim_targets[hand];
    }
    return targets;
}
bool Presentation::reticles_active(const GameState& state) const {
    const auto& p=*impl_;return p.input.focused&&!p.opened&&!p.service_mode&&!p.startup_help.show(p.help,p.opened,p.service_mode)&&state.valid&&state.playing;
}
void Presentation::update(const PresentationInput& input,Settings& s,SharedRuntime& runtime,double now) {
    auto& p=*impl_;p.input=input;p.runtime=&runtime;
    {std::lock_guard lock(runtime.mutex);
        const auto& state=runtime.state;p.native_state=state;const bool was_service=p.service_mode;
        if(state.valid&&!runtime.game_options_busy) {
            if(state.service){p.service_seen_native=true;p.service_mode=true;}
            else if(p.service_seen_native){p.service_mode=false;p.service_seen_native=false;}
            const bool busy=state.gun_calibration&&state.service_busy!=0;
            if(state.gun_calibration!=p.native_calibration||busy!=p.calibration_busy)p.service_guns=busy;
            p.native_calibration=state.gun_calibration;
            p.calibration_busy=busy;
            if(!p.service_mode)p.service_guns=false;
            if(was_service!=p.service_mode){p.shot_time={-100,-100};p.gun_effects.clear();}
        }
    }
    if(!input.focused)suspend();
    else if(!input.hands[0].tracked&&!input.hands[1].tracked){p.gun_effects.clear();p.shot_time={-100,-100};}
    bool trigger_edge[2],start_edge[2];
    for(int h=0;h<2;h++){trigger_edge[h]=p.trigger_latch[h].edge(input.hands[h].trigger,input.focused&&input.hands[h].tracked);start_edge[h]=p.start_latch[h].edge(input.hands[h].start,input.focused);}
    runtime.game_options_focused=input.focused;
    bool restart_complete=false;
    {
        std::lock_guard lock(runtime.mutex);bool finished=p.game_busy&&!runtime.game_options_busy;
        p.game_busy=runtime.game_options_busy;p.game_status=runtime.game_options_status;p.game_error=runtime.game_options_error;p.game_count=runtime.game_options_count;p.game_index=runtime.game_options_index;
        p.options=runtime.game_options;if(finished&&p.game_status==GameOptionsStatus::Complete){p.game_edits.clear();restart_complete=true;}p.edit_options=p.game_edits.merge(p.options);p.game_dirty=p.game_edits.dirty(p.options);
    }
    if(restart_complete){p.opened=false;p.help=false;p.pointer.suspend();p.fresh=true;p.stick_wait=true;runtime.release_inputs();}
    if(p.toggle_latch.edge(input.toggle,input.focused)&&!p.game_busy){
        p.opened=!p.opened;p.fresh=true;p.help=false;p.quit_armed=false;p.confirm=Impl::Confirm::None;
        p.focus(p.actions().front());p.stick_wait=true;runtime.release_inputs();
        auto f=glm::vec3(input.head*glm::vec4(0,0,-1,0));auto at=glm::vec3(input.head[3]);
        p.panel_model=glm::translate(glm::mat4(1),at)*glm::rotate(glm::mat4(1),std::atan2(-f.x,-f.z),glm::vec3(0,1,0))*glm::translate(glm::mat4(1),glm::vec3(0,-.1,-1.2));
        p.trace(p.opened?"open":"close");
    }
    std::array<bool,2> tracked{input.hands[0].tracked,input.hands[1].tracked},pressed{input.hands[0].trigger,input.hands[1].trigger};
    auto pointer=p.pointer.update(p.opened,input.focused,tracked,pressed,s.p1_hand);
    if(pointer.changed){p.hovered[0]=p.hovered[1]=-1;p.stick_wait=true;p.stick_was={};}
    glm::vec2 active_stick=pointer.owner>=0?input.sticks[pointer.owner]:glm::vec2(0);
    if(p.opened&&input.focused){
        if(pointer.owner>=0){
            int h=pointer.owner;auto panel=p.panel(s);auto aim=p.help?input.poses[h]:pitched_gun_pose(input.poses[h],s.gun_angle);
            auto origin=p.help?glm::vec3(aim[3]):muzzle(aim,s),direction=glm::vec3(aim*glm::vec4(0,0,-1,0));
            auto hit=vr::xr::settingsHit(p.panel_model,panel,origin,direction);using K=vr::xr::SettingsHit::Kind;
            if(trigger_edge[0]||trigger_edge[1])p.trace("trigger",hit,pointer.changed);
            MenuAction action{};bool valid=true;
            if(hit.kind==K::Close)action=MenuAction::Close;
            else if(hit.kind==K::Back)action=MenuAction::Back;
            else if(hit.kind==K::Tab&&hit.index>=0&&hit.index<4)action=MenuTabs[hit.index];
            else if(hit.kind==K::Row&&hit.index>=0&&size_t(hit.index)<p.actions().size())action=p.actions()[hit.index];else valid=false;
            int nav=-1;if(valid){auto n=p.navigation();auto at=std::find(n.begin(),n.end(),action);nav=int(at-n.begin());}
            if(valid&&!pointer.changed){if(nav!=p.hovered[h])p.selected=nav;if(pointer.activate&&!p.fresh){
                bool actionable=hit.kind!=K::Row||panel.rows[hit.index].action||panel.rows[hit.index].stepper||!panel.rows[hit.index].choices.empty();
                if(actionable){p.choose(action,hit.direction?hit.direction:1,hit.choice,s,runtime,now);p.trace("action",hit,pointer.changed);}
            }}
            p.hovered[h]=nav;
        }
        if(glm::length(active_stick)<.25f)p.stick_wait=false;
        auto stick=p.stick_wait?glm::vec2(0):active_stick;
        bool vertical=std::abs(stick.y)>.65f,horizontal=std::abs(stick.x)>.65f,step=now>=p.repeat_at;
        if(vertical&&(std::abs(p.stick_was.y)<.65f||step)){auto n=p.navigation();p.selected=(p.selected+(stick.y>0?-1:1)+int(n.size()))%int(n.size());p.repeat_at=now+(std::abs(p.stick_was.y)<.65f?.4:.15);}
        if(horizontal&&(std::abs(p.stick_was.x)<.65f||step)){
            auto action=p.selected_action();auto rows=p.actions();auto at=std::find(rows.begin(),rows.end(),action);
            if(at!=rows.end()){auto panel=p.panel(s);const auto& row=panel.rows[at-rows.begin()];if(!row.dim&&!row.action&&(row.stepper||!row.choices.empty()))p.choose(action,stick.x>0?1:-1,-1,s,runtime,now);}
            p.repeat_at=now+.15;
        }
        if(pointer.owner>=0&&start_edge[pointer.owner]&&!p.fresh&&!pointer.changed)p.choose(p.selected_action(),1,-1,s,runtime,now);
        p.fresh=false;
    }
    p.stick_was=active_stick;
}
void Presentation::draw(const glm::mat4& vp,const Settings& s,const GameState& state,bool ready,double now,bool splash) {
    auto& p=*impl_;
    bool show_help=p.startup_help.show(p.help,p.opened,p.service_mode);
    if(splash||!ready) {vr::xr::Panel panel;panel.brand=true;vr::xr::drawPanel(vp,panel);if(splash)return;}
    prepare(s);
    if(show_help&&!p.controllers_loaded) {p.controllers_loaded=true;for(int h=0;h<2;h++) {if(p.controllers[h].file(h==0?"models/controllers/left.glb":"models/controllers/right.glb",false))p.controllers[h].placement=glm::rotate(glm::mat4(1),glm::pi<float>(),glm::vec3(0,1,0));else p.controllers[h].arcade();}}
    p.init_model();bool depth_cleared=false;
    struct Effect {glm::vec3 muzzle,direction,target,flash_muzzle,flash_direction;glm::vec4 tint;double since;int player,hand;};
    std::vector<Effect> effects;effects.reserve(2);
    const bool shot_effects=!show_help&&!p.opened&&!p.service_mode&&p.input.focused;
    if(!shot_effects)p.gun_effects.clear();
    for(int player=0;player<2;player++) {
        int hand=p.opened||p.service_mode||show_help?player:p.cabinet.hand[player];if(hand<0||!p.input.hands[hand].tracked)continue;
        if(!p.opened&&!p.service_mode&&!show_help&&(!state.valid||!state.playing||!state.players[player].alive()))continue;
        auto hand_pose=p.input.poses[hand];double since=now-p.shot_time[player];float kick=since>=0&&since<.18?float(std::sin(since/.18*glm::pi<double>())*std::exp(-since*12)):0;
        auto rest=pitched_gun_pose(hand_pose,s.gun_angle);
        auto& mesh=show_help?p.controllers[hand]:p.gun;
        auto kicked=rest*glm::translate(glm::mat4(1),glm::vec3(0,0,kick*.035f))*glm::rotate(glm::mat4(1),kick*.12f,glm::vec3(1,0,0));
        auto model=(show_help?hand_pose:kicked*glm::scale(glm::mat4(1),glm::vec3(s.gun_scale)))*mesh.placement;
        if(!depth_cleared){glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);depth_cleared=true;}
        glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glUseProgram(p.model_program);glBindVertexArray(mesh.vao);
        auto mvp=vp*model;glUniformMatrix4fv(glGetUniformLocation(p.model_program,"mvp"),1,GL_FALSE,&mvp[0][0]);glUniformMatrix4fv(glGetUniformLocation(p.model_program,"model"),1,GL_FALSE,&model[0][0]);
        glActiveTexture(GL_TEXTURE0);glUniform1i(glGetUniformLocation(p.model_program,"tex"),0);
        for(const auto& part:mesh.mesh.parts) {
            bool textured=part.image>=0&&size_t(part.image)<mesh.textures.size();glUniform1i(glGetUniformLocation(p.model_program,"textured"),textured);if(textured)glBindTexture(GL_TEXTURE_2D,mesh.textures[part.image]);
            const float* base=part.color;glm::vec4 plastic;
            if(!show_help&&mesh.arcade_model&&(part.firstIndex==vr::xr::gunmodel::Parts[0].first||part.firstIndex==vr::xr::gunmodel::Parts[1].first)){
                int owner=player;
                if(p.opened||p.service_mode){owner=hand==s.p1_hand?0:1;for(int candidate=0;candidate<2;candidate++)if(p.cabinet.hand[candidate]==hand)owner=candidate;}
                plastic=owner==0?glm::vec4(.90f,.04f,.04f,1):glm::vec4(.04f,.25f,1.f,1);base=&plastic[0];
            }
            glUniform4fv(glGetUniformLocation(p.model_program,"base"),1,base);glDrawElements(GL_TRIANGLES,part.indexCount,GL_UNSIGNED_INT,reinterpret_cast<void*>(uintptr_t(part.firstIndex*4)));
        }
        auto muzzle=show_help?glm::vec3(hand_pose[3]):this->muzzle(rest,s);auto direction=glm::normalize(glm::vec3((show_help?hand_pose:rest)*glm::vec4(0,0,-1,0)));auto target=muzzle+direction*3.f;
        if(!p.opened&&!show_help)if(auto hit=p.input.aim_targets[hand])target={float(hit->x),float(hit->y),float(hit->z)};
        if(p.opened)vr::xr::settingsPanelHit(p.panel_model,p.panel(s),muzzle,direction,&target);
        glm::vec4 tint=player==0?glm::vec4(1,.08,.05,.8):glm::vec4(.15,.45,1,.8);if(p.opened)tint={.66,.61,1,.8};
        auto flash_muzzle=glm::vec3(model*glm::vec4(mesh.muzzle,1)),flash_direction=glm::normalize(glm::vec3(kicked*glm::vec4(0,0,-1,0)));
        if(shot_effects)p.gun_effects.emit(player,p.shot_time[player],now,muzzle,direction,s.gun_scale);
        effects.push_back({muzzle,direction,target,flash_muzzle,flash_direction,tint,since,player,hand});
    }
    // CM's beam faces each eye; a fixed world-axis strip can become edge-on.
    auto eye_h=glm::inverse(vp)*glm::vec4(0,0,1,0);
    auto eye=std::abs(eye_h.w)>1e-6f?glm::vec3(eye_h)/eye_h.w:glm::vec3(p.input.head[3]);
    // Both opaque guns establish depth before either player's translucent effects.
    for(const auto& effect:effects) {
        if(p.opened&&effect.hand!=p.pointer.owner())continue;
        const auto& muzzle=effect.muzzle;const auto& direction=effect.direction;const auto& target=effect.target;const auto& tint=effect.tint;
        if((s.laser&&!show_help)||p.opened) {
            p.gun_effects.laser(vp,eye,muzzle,target,presentation_laser_half_width(s.laser_thickness,p.opened||p.service_mode||show_help),glm::vec3(tint));
            if(!p.opened&&(!reticles_active(state)||s.reticle_mode==ReticleMode::Ingame))vr::xr::drawTexture(vp,glm::translate(glm::mat4(1),target),0,.006f,.006f,tint,true);
        }
    }
    if(shot_effects) {
        p.gun_effects.smoke(vp,eye,now);
        for(const auto& e:effects)p.gun_effects.flash(vp,eye,e.player,e.since,e.flash_muzzle,e.flash_direction,s.gun_scale);
    }
    if(p.opened) {
        vr::xr::drawSettingsPanel(vp,p.panel_model,p.panel(s));
        int hand=p.pointer.owner();
        if(p.input.focused&&hand>=0&&p.input.hands[hand].tracked){
            auto aim=show_help?p.input.poses[hand]:pitched_gun_pose(p.input.poses[hand],s.gun_angle);
            auto origin=show_help?glm::vec3(aim[3]):muzzle(aim,s);auto direction=glm::vec3(aim*glm::vec4(0,0,-1,0));glm::vec3 hit;
            if(vr::xr::settingsPanelHit(p.panel_model,p.panel(s),origin,direction,&hit))p.gun_effects.contact(vp,eye,hit);
        }
    }
    if(p.service_mode&&!p.opened) {
        int width{},height{};auto text=!p.service_seen_native?"Entering native Service Menu...":p.service_guns?"AIM SET  Trigger: native aim setting   A / X: native Start   Right stick click: menu controls":"SERVICE  Stick down / B / Y: next   Trigger / A / X: Test select / exit";
        auto texture=vr::xr::textTexture(text,24,vr::xr::theme::Text,width,height);
        vr::xr::drawTexture(vp,glm::translate(glm::mat4(1),glm::vec3(0,-.65f,-1.8f)),texture,width*.00065f,height*.00065f);
    }
    // As in Confidential Mission, labels go over the scene, controllers and menu.
    if(show_help) {
        for(int hand=0;hand<2;hand++)if(p.input.hands[hand].tracked) {
            p.controller_labels.draw(vp,hand,p.input.poses[hand]*p.controllers[hand].placement,controller_help::words(hand,s,state,p.opened&&!p.help,p.service_mode,p.service_guns));
        }
    }
}
}
