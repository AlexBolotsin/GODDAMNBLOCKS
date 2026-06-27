#pragma once

#include "Entity.h"
#include <vector>
#include <memory>

struct SceneParticle
{
    float x, y, z; // world position
    float size;     // world-space radius
    float r, g, b, a;
};

class Scene
{
public:
    Scene() = default;
    ~Scene() = default;

    Entity& CreateEntity();
    const std::vector<std::unique_ptr<Entity>>& GetEntities() const { return m_entities; }
    std::vector<std::unique_ptr<Entity>>& GetEntities() { return m_entities; }

    std::vector<SceneParticle>&       GetParticles()       { return m_particles; }
    const std::vector<SceneParticle>& GetParticles() const { return m_particles; }

    void Clear();

private:
    std::vector<std::unique_ptr<Entity>> m_entities;
    std::vector<SceneParticle>           m_particles;
};
