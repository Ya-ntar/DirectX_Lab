#include "Scene.h"

#include <algorithm>

namespace gfw {
    namespace {
        DirectX::XMFLOAT3 TranslationOf(const DirectX::XMFLOAT4X4 &m) {
            return {m._41, m._42, m._43};
        }

        float DistanceSq(const DirectX::XMFLOAT3 &a, const DirectX::XMFLOAT3 &b) {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            const float dz = a.z - b.z;
            return dx * dx + dy * dy + dz * dz;
        }
    }

    Scene::Entity &Scene::CreateEntity(const MeshBuffers *mesh, std::shared_ptr<Texture2D> texture) {
        entities_.push_back({});
        Entity &e = entities_.back();
        e.render.mesh = mesh;
        e.render.texture = std::move(texture);
        return e;
    }

    void Scene::Update(float time_seconds, float dt_seconds) {
        for (Entity &e: entities_) {
            if (e.behavior) {
                e.behavior(e.render, time_seconds, dt_seconds);
            }
        }
    }

    void Scene::Render(Framework &framework, double total_time) const {
        struct DrawItem {
            const RenderObject *object = nullptr;
            float sort_key = 0.0f;
            bool transparent = false;
        };

        std::vector<DrawItem> items;
        items.reserve(entities_.size());

        const DirectX::XMFLOAT3 camera_pos = framework.GetSceneState().camera.position;

        for (const Entity &e: entities_) {
            if (!e.render.mesh) {
                continue;
            }
            const bool transparent = e.render.albedo.w < 0.999f;
            const DirectX::XMFLOAT3 pos = TranslationOf(e.render.world);
            const float dist_sq = DistanceSq(pos, camera_pos);
            items.push_back({&e.render, dist_sq, transparent});
        }

        std::stable_sort(items.begin(), items.end(), [](const DrawItem &a, const DrawItem &b) {
            if (a.transparent != b.transparent) {
                return !a.transparent && b.transparent;
            }
            if (a.transparent) {
                return a.sort_key > b.sort_key;
            }
            return a.sort_key < b.sort_key;
        });

        for (const DrawItem &item: items) {
            framework.RenderObject(*item.object, total_time);
        }
    }
}

