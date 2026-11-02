// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "shared_runtime.h"
#include "menu_model.h"
#include <glm/glm.hpp>
#include <memory>
namespace gunblade {
struct PresentationInput {
    Hands hands;
    std::array<std::optional<Vec3>,2> aim_targets{};
    std::array<glm::mat4,2> poses{glm::mat4(1),glm::mat4(1)};
    glm::mat4 head{1};glm::vec2 stick{};std::array<glm::vec2,2> sticks{};
    bool toggle{};
    bool focused{true};
};
class Presentation {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    Presentation();~Presentation();
    bool open() const;
    MenuPage menu_page() const;
    MenuAction selected_action() const;
    int menu_pointer_hand() const;
    bool service() const;
    bool service_guns() const;
    void toggle_service_guns();
    void prepare(const Settings&,SharedRuntime* runtime=nullptr);
    void suspend();
    bool recenter_requested();
    glm::vec3 muzzle(const glm::mat4& aim_pose,const Settings&) const;
    void update(const PresentationInput&,Settings&,SharedRuntime&,double now);
    void feedback(const CabinetInput&,double now);
    std::array<std::optional<Vec3>,2> reticle_targets(const GameState&) const;
    bool reticles_active(const GameState&) const;
    void draw(const glm::mat4& vp,const Settings&,const GameState&,bool scene_ready,double now,bool splash=false);
};
}
