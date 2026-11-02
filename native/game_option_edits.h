// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "game_options.h"
namespace gunblade {
// UI-only staging. Never changes native RAM/storage; merge only touched fields
// onto the newest snapshot, retaining unrelated manual Service changes.
class GameOptionEdits {
 std::array<uint32_t,4> values_{};unsigned mask_{};
public:
 void clear(){mask_=0;}
 void stage(const GameOptions& native,GameOption option,uint32_t value){
  if(!native.valid||!game_option_value_valid(option,value))return;
  unsigned n=unsigned(option),bit=1u<<n;
  if(value==native.value(option))mask_&=~bit;else {values_[n]=value;mask_|=bit;}
 }
 GameOptions merge(const GameOptions& native) const{
  auto out=native;uint32_t* fields[]={&out.difficulty,&out.shifting_difficulty,&out.player_life,&out.attract_sound};
  for(unsigned n=0;n<4;n++)if(mask_&(1u<<n))*fields[n]=values_[n];return out;
 }
 bool dirty(const GameOptions& native) const{
  for(unsigned n=0;n<4;n++)if((mask_&(1u<<n))&&values_[n]!=native.value(GameOption(n)))return true;return false;
 }
};
}
