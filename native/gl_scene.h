// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model2_scene.h"
#include "native_reticle.h"
#include "reticle_mode.h"
#include <glm/glm.hpp>
#include <memory>
namespace gunblade {
class GLScene {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    GLScene();~GLScene();
    void set_hud_size(float size,bool gameplay,bool service=false);
    // Optional camera-space prototype. Native/reference presentation is default.
    void set_immersive(bool enabled);
    void prepare(std::shared_ptr<const NativeFrame> frame,bool wider_view=false);
    void set_presentation_lens(const NativeLens& lens);
    // One shared head-facing native marker at each actual scene aim target.
    // A missing target preserves that player's original native reticle.
    void set_reticle_targets(const std::array<std::optional<Vec3>,2>& targets,const glm::mat4& head);
    void set_reticle_size(float minimum_ratio,float shrink_distance);
    void set_reticle_sizes(float ingame_min,float ingame_max,float custom_min,float custom_max,float shrink_distance);
    void set_reticle_mode(ReticleMode mode,bool gameplay);
    const NativeReticleDiagnostics& reticle_diagnostics() const;
    std::shared_ptr<const NativeFrame> prepared_frame() const;
    SceneAim trace_aim(Ray ray,float world_scale,float panel_distance) const;
    void draw(const glm::mat4& view_projection,float world_scale,float panel_distance);
};
}
