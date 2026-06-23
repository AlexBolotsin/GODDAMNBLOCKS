#pragma once

#include "Entity.h"
#include <vector>
#include <memory>

class Scene
{
public:
    Scene() = default;
    ~Scene() = default;

    Entity& CreateEntity();
    const std::vector<std::unique_ptr<Entity>>& GetEntities() const { return m_entities; }
    std::vector<std::unique_ptr<Entity>>& GetEntities() { return m_entities; }

    void Clear();

private:
    std::vector<std::unique_ptr<Entity>> m_entities;
};
