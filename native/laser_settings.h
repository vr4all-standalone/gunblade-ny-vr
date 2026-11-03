// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "world_presets.h"

namespace gunblade {
// Keep stored width indices 0/1 for Normal, 2 for 5x and 3 for 10x.
inline constexpr int laser_menu_choice(bool enabled, int stored) {
    if (!enabled) return 0;
    return stored == 3 ? 3 : stored == 2 ? 2 : 1;
}
inline constexpr int laser_stored_choice(int choice) {
    return choice == 3 ? 3 : choice == 2 ? 2 : 0;
}
inline float presentation_laser_half_width(int stored, bool pointer) {
    return laser_half_width(pointer ? 0 : (stored == 2 || stored == 3 ? stored : 1), pointer);
}
}
