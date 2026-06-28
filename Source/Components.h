#pragma once
#include "ECS.h"
#include "Transform.h"
#include "Mesh.h"
#include "Material.h"
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
//  Component structs — plain data, no logic
//  Add fields freely; add a new type by declaring a ComponentPool<T> in World
// ---------------------------------------------------------------------------

struct TransformComp
{
    Transform transform;
};

struct RenderComp
{
    std::shared_ptr<Mesh>     mesh;
    std::shared_ptr<Material> material;
    vec4 tint                 = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    bool visible              = true;
    bool isBillboard          = false;
    bool castsProjectedShadow = true;
    bool isUnlit              = false;
    bool isInstanced          = false;  // GPU-instanced bulk draw
    bool isBlobShadow         = false;  // flat shadow circle on floor
    bool usesSpriteTexture    = false;  // sample sprite atlas in shader
};

struct SpriteComp
{
    vec4               uvRect     = vec4(0.0f, 0.0f, 1.0f, 1.0f); // current atlas sub-rect
    float              hoverPhase = 0.0f;                           // per-sprite GPU hover offset
    std::vector<vec4>  animFrames;                                  // atlas rects for each frame
    float              animSpeed  = 8.0f;
    float              animTimer  = 0.0f;
};

struct PhysicsComp
{
    vec3 velocity = vec3(0.0f, 0.0f, 0.0f);
};

struct BurningComp
{
    float age      = 0.0f;
    float duration = 1.0f;
    vec4  origTint = vec4(1.0f, 1.0f, 1.0f, 1.0f); // colour before fire started
};
