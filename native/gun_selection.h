// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace gunblade {
inline int normalize_gun(int value){return value==1?1:0;}
inline int next_gun(int value,int /*direction*/){return 1-normalize_gun(value);}
inline const char* gun_label(int value){return normalize_gun(value)==1?"Zapper":"Arcade";}
}
