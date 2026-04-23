#include "SceneAnimation.h"

#include <algorithm>

namespace gfw {

void ApplyVertexAnimationParams(const SceneObjectConfig &object_config, RenderObject &render_object) {
    if (object_config.vertex_wave_amplitude <= 0.0f) {
        render_object.effect_params.z = 0.0f;
        render_object.effect_params.w = 0.0f;
        return;
    }

    render_object.effect_params.z = object_config.vertex_wave_amplitude;
    render_object.effect_params.w = (std::max)(object_config.vertex_wave_frequency, 0.001f);
}

}  // namespace gfw

