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

struct SceneShockwave
{
    float x, y, z;  // world position of the impact
    float age;       // seconds since birth
    float maxAge;    // total lifetime
};

class Scene
{
public:
    Scene() = default;
    ~Scene() = default;

    Entity& CreateEntity();
    const std::vector<std::unique_ptr<Entity>>& GetEntities() const { return m_entities; }
    std::vector<std::unique_ptr<Entity>>& GetEntities() { return m_entities; }

    std::vector<SceneParticle>&        GetParticles()        { return m_particles; }
    const std::vector<SceneParticle>&  GetParticles()  const { return m_particles; }

    std::vector<SceneShockwave>&       GetShockwaves()       { return m_shockwaves; }
    const std::vector<SceneShockwave>& GetShockwaves() const { return m_shockwaves; }

    float GetTime() const  { return m_time; }
    void  SetTime(float t) { m_time = t; }

    void Clear();

private:
    std::vector<std::unique_ptr<Entity>> m_entities;
    std::vector<SceneParticle>           m_particles;
    std::vector<SceneShockwave>          m_shockwaves;
    float                                m_time = 0.0f;
};
