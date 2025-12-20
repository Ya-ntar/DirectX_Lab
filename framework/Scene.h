#pragma once

#include <functional>
#include <vector>
#include "Framework.h"

namespace gfw {
    class Scene {
    public:
        struct Entity {
            RenderObject render = {};
            std::function<void(RenderObject &object, float time_seconds, float dt_seconds)> behavior = {};
        };

        Entity &CreateEntity(const MeshBuffers *mesh, std::shared_ptr<Texture2D> texture);

        void Update(float time_seconds, float dt_seconds);

        void Render(Framework &framework, double total_time) const;

        [[nodiscard]] std::vector<Entity> &Entities() { return entities_; }

        [[nodiscard]] const std::vector<Entity> &Entities() const { return entities_; }

    private:
        std::vector<Entity> entities_;
    };
}

