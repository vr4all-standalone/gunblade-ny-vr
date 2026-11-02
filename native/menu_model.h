// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <vector>
#include <string>
#include <array>
namespace gunblade {
enum class MenuPage {Gun,World,System,Game,GunAdvanced,WorldAdvanced};
enum class MenuAction {
 GunModel,GunAngle,Laser,Reticle,Haptics,OpenGunAdvanced,GunSize,ReticleMinimum,ReticleMaximum,ReticleDistance,
 WiderView,OpenWorldAdvanced,WorldScale,HudSize,PanelDistance,ViewWidth,
 TwoPlayers,OpenService,HowToPlay,ResetDefaults,Quit,GameDifficulty,GameShifting,GameLives,GameAttractSound,
 Back,Close,Confirm,Cancel,TabGun,TabWorld,TabSystem,TabGame,ApplyGameSettings
};
inline MenuPage main_page(MenuPage p){return p==MenuPage::GunAdvanced?MenuPage::Gun:p==MenuPage::WorldAdvanced?MenuPage::World:p;}
inline constexpr std::array<MenuAction,4> MenuTabs{MenuAction::TabGun,MenuAction::TabWorld,MenuAction::TabSystem,MenuAction::TabGame};
inline int menu_tab_index(MenuPage page){switch(main_page(page)){case MenuPage::Gun:return 0;case MenuPage::World:return 1;case MenuPage::System:return 2;default:return 3;}}
// Keep original initialization advancing behind an early menu. Focus loss
// still pauses, as do loaded attract, active play and native Service.
template<class State> inline bool menu_pauses_native(bool open,bool focused,const State& state){return !focused||(open&&state.valid&&(state.ready||state.playing||state.service));}
inline MenuPage menu_tab_page(MenuAction action){switch(action){case MenuAction::TabGame:return MenuPage::Game;case MenuAction::TabWorld:return MenuPage::World;case MenuAction::TabSystem:return MenuPage::System;default:return MenuPage::Gun;}}
inline std::vector<MenuAction> menu_actions(MenuPage p,const std::vector<MenuAction>& native={}){
 using A=MenuAction;
 switch(p){case MenuPage::Gun:return {A::GunModel,A::GunAngle,A::Laser,A::Reticle,A::Haptics,A::OpenGunAdvanced};
 case MenuPage::GunAdvanced:return {A::GunSize,A::ReticleMinimum,A::ReticleMaximum,A::ReticleDistance};
 case MenuPage::World:return {A::WiderView,A::OpenWorldAdvanced};
 case MenuPage::WorldAdvanced:return {A::WorldScale,A::HudSize,A::PanelDistance,A::ViewWidth};
 case MenuPage::System:return {A::TwoPlayers,A::OpenService,A::HowToPlay,A::ResetDefaults,A::Quit};
 case MenuPage::Game:return native.empty()?std::vector<MenuAction>{A::OpenService}:native;}
 return {};
}
inline const char* menu_label(MenuAction a){
 switch(a){case MenuAction::GunModel:return "Gun model";case MenuAction::GunAngle:return "Gun angle";case MenuAction::Laser:return "Laser";case MenuAction::Reticle:return "Crosshair";case MenuAction::Haptics:return "Haptics";
 case MenuAction::OpenGunAdvanced:return "Advanced gun settings";case MenuAction::GunSize:return "Gun size";case MenuAction::ReticleMinimum:return "Crosshair min size";case MenuAction::ReticleMaximum:return "Crosshair max size";case MenuAction::ReticleDistance:return "Crosshair shrink distance";
 case MenuAction::WiderView:return "Wider view";case MenuAction::OpenWorldAdvanced:return "Advanced world settings";case MenuAction::WorldScale:return "World size";case MenuAction::HudSize:return "HUD size";case MenuAction::PanelDistance:return "Panel distance";case MenuAction::ViewWidth:return "View width";
 case MenuAction::TwoPlayers:return "Two players";case MenuAction::OpenService:return "Open Service Menu";case MenuAction::HowToPlay:return "How to play";case MenuAction::ResetDefaults:return "Reset to defaults";case MenuAction::Quit:return "Quit game";
 case MenuAction::GameDifficulty:return "Difficulty";case MenuAction::GameShifting:return "Shifting difficulty";case MenuAction::GameLives:return "Player life";case MenuAction::GameAttractSound:return "Attract sound";
 case MenuAction::Back:return "Back";case MenuAction::Close:return "Close";case MenuAction::Confirm:return "Confirm";case MenuAction::Cancel:return "Cancel";
 case MenuAction::TabGun:return "Gun";case MenuAction::TabWorld:return "World";case MenuAction::TabSystem:return "System";case MenuAction::TabGame:return "In-game";case MenuAction::ApplyGameSettings:return "Apply settings and restart";}return "";
}
}
