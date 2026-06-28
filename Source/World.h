#pragma once
#include "ECS.h"
#include "Components.h"
#include <vector>

// Non-entity scene data — kept as plain flat structs, no ECS overhead needed
struct SceneParticle  { float x, y, z, size, r, g, b, a; };
struct SceneShockwave { float x, y, z, age, maxAge; };

// World — owns all entities and their component pools.
//
// Creating an entity:
//   EntityID id = world.CreateEntity();
//   world.transforms.Add(id).transform.SetPosition(1,2,3);
//   world.renders.Add(id, RenderComp{ mesh, material });
//
// Systems iterate a pool directly:
//   for (size_t i = 0; i < world.physics.Size(); ++i) {
//       EntityID    id   = world.physics.IdAt(i);
//       PhysicsComp& pc  = world.physics.DataAt(i);
//       TransformComp* xf = world.transforms.Get(id);
//       ...
//   }
//   // or with a callback:
//   world.physics.ForEach([&](EntityID id, PhysicsComp& pc) { ... });
//
// Adding a new component type:
//   1. Declare a struct in Components.h
//   2. Add a ComponentPool<YourComp> member here

class World
{
public:
    EntityID CreateEntity();
    void     DestroyEntity(EntityID id);  // removes from every pool
    void     Clear();                     // wipes everything, resets IDs

    // Component pools — one per type
    ComponentPool<TransformComp> transforms;
    ComponentPool<RenderComp>    renders;
    ComponentPool<SpriteComp>    sprites;
    ComponentPool<PhysicsComp>   physics;
    ComponentPool<BurningComp>   burning;

    // Flat non-entity data
    std::vector<SceneParticle>   particles;
    std::vector<SceneShockwave>  shockwaves;
    float                        time = 0.0f;

private:
    uint32_t m_nextId = 1; // 0 is kNullEntity, so we start at 1
};
