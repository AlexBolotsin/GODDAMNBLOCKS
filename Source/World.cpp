#include "World.h"

EntityID World::CreateEntity()
{
    return m_nextId++;
}

void World::DestroyEntity(EntityID id)
{
    transforms.Remove(id);
    renders.Remove(id);
    sprites.Remove(id);
    physics.Remove(id);
    burning.Remove(id);
}

void World::Clear()
{
    transforms.Clear();
    renders.Clear();
    sprites.Clear();
    physics.Clear();
    burning.Clear();
    particles.clear();
    shockwaves.clear();
    time    = 0.0f;
    m_nextId = 1;
}
