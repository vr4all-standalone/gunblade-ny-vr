// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
namespace gunblade {
// The overseer's displayed zero retains the original physical -10 degree pose.
inline glm::mat4 pitched_gun_pose(const glm::mat4& pose,float displayed_angle) {
    const float angle=std::isfinite(displayed_angle)?std::clamp(displayed_angle,-30.f,30.f):0.f;
    return pose*glm::rotate(glm::mat4(1),glm::radians(angle-10.f),glm::vec3(1,0,0));
}
}
