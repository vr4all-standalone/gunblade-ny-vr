// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cmath>
#include "game_state.h"
namespace gunblade {
// Gunblade main mode2 also covers mission selection(1) and stage intro(6).
// HUD resizing is restricted to the verified active-stage submode(2).
inline bool hud_active_stage(const GameState& state){return state.valid&&state.playing&&state.submode==2;}
// Only identified native UI in the active spatial gameplay view is supported.
// Unknown embedded HUD meshes and complete native raster fallbacks stay native.
inline float effective_hud_size(float saved,bool large,bool gameplay,bool service,bool usable){
    return large&&gameplay&&!service&&usable&&std::isfinite(saved)&&saved>=.5f&&saved<=2.f?saved:1.f;
}
}
