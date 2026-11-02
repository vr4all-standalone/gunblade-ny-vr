#include "../native/presentation.h"
#include "../native/gl_context.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace gunblade;
int main(int argc,char** argv){
 assert(argc==3);_putenv_s("GUNBLADE_ASSETS",argv[1]);_putenv_s("GUNBLADE_SETTINGS","menu-capture-test.ini");GLContext gl;
 constexpr int w=1200,h=880;GLuint texture{},fbo{};glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
 glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glViewport(0,0,w,h);
 Presentation ui;Settings s;SharedRuntime r;r.game_options={true,true,3,3,3,1};r.state.valid=r.state.ready=true;PresentationInput in;in.hands[0].tracked=true;double now{};
 auto tick=[&](){now+=1;ui.update(in,s,r,now);};auto step=[&](glm::vec2 v){in.sticks[0]=v;tick();in.sticks[0]={};tick();};
 auto seek=[&](MenuAction a){for(int i=0;i<32&&ui.selected_action()!=a;i++)step({0,-1});assert(ui.selected_action()==a);};
 auto select=[&](){in.hands[0].start=true;tick();in.hands[0].start=false;tick();};
 tick();in.toggle=true;tick();in.toggle=false;tick();
 auto vp=glm::ortho(-.565f,.565f,-.5143333f,.3143333f,.05f,10.f);
 std::filesystem::create_directories(argv[2]);
 auto capture=[&](const char* name){
  glClearColor(.035f,.045f,.06f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);ui.draw(vp,s,GameState{},true,now,false);assert(glGetError()==GL_NO_ERROR);
  std::vector<unsigned char> pixels(w*h*4);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
  std::ofstream out(std::filesystem::path(argv[2])/(std::string(name)+".bmp"),std::ios::binary);
  auto u16=[&](uint16_t n){out.put(char(n));out.put(char(n>>8));};auto u32=[&](uint32_t n){u16(n);u16(n>>16);};
  out.write("BM",2);u32(54+w*h*3);u32(0);u32(54);u32(40);u32(w);u32(h);u16(1);u16(24);u32(0);u32(w*h*3);u32(0);u32(0);u32(0);u32(0);
  for(size_t i=0;i<pixels.size();i+=4){out.put(char(pixels[i+2]));out.put(char(pixels[i+1]));out.put(char(pixels[i]));}assert(out);std::cout<<"Captured actual Presentation "<<name<<'\n';
 };
 capture("gun");seek(MenuAction::Laser);step({1,0});step({1,0});step({1,0});capture("gun-10x");step({-1,0});step({-1,0});step({-1,0});step({-1,0});capture("gun-off");step({1,0});seek(MenuAction::OpenGunAdvanced);select();capture("gun-advanced");
 seek(MenuAction::TabWorld);select();capture("world");seek(MenuAction::WiderView);step({1,0});capture("world-fade");seek(MenuAction::OpenWorldAdvanced);select();capture("world-advanced");
 seek(MenuAction::TabSystem);select();capture("system");seek(MenuAction::ResetDefaults);select();capture("reset");seek(MenuAction::Cancel);select();
 seek(MenuAction::Quit);select();capture("quit");seek(MenuAction::Cancel);select();
 seek(MenuAction::TabGame);select();capture("game");seek(MenuAction::GameDifficulty);step({1,0});
 r.game_options.eligible=false;tick();capture("game-staged-active");r.game_options.eligible=true;tick();capture("game-staged-ready");
 seek(MenuAction::ApplyGameSettings);select();assert(r.game_options_busy);capture("game-restarting");
 r.game_options_status=GameOptionsStatus::Saving;r.game_options_count=1;tick();capture("game-saving");
 std::filesystem::remove("menu-capture-test.ini");glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&texture);_putenv_s("GUNBLADE_SETTINGS","");
}
