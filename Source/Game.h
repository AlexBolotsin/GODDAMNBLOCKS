#pragma once
#include "Camera.h"
#include "InputState.h"
#include "World.h"
#include <memory>
#include <vector>

class DX12Context;
class Mesh;
class Material;

class Game
{
    // ---------------------------------------------------------------------------
    //  Private data types — game-logic state that outlives a single Update tick
    // ---------------------------------------------------------------------------

    struct ExplosionData
    {
        EntityID sphere;    // visual sphere entity (destroyed when explosion ends)
        vec3     center;
        float    age;
        float    maxAge;
        float    maxRadius;
    };

    struct FireParticle
    {
        vec3  pos;
        vec3  vel;
        float age;
        float maxAge;
        float startSize;
    };

public:
    bool Init(DX12Context& dx12, const wchar_t* shaderPath, const wchar_t* spriteSheetPath);
    void Update(float dt, const InputState& input);

    World&        GetWorld()          { return m_world; }
    const Camera& GetCamera()  const  { return m_camera; }

private:
    World    m_world;
    Camera   m_camera;
    float    m_time          = 0.0f;
    bool     m_cinematicMode = false;
    float    m_cinematicTime = 0.0f;

    float m_camAzimuth   = 0.0f;
    float m_camElevation = 0.245f;
    float m_camRadius    = 8.25f;

    // Targeting gizmo
    EntityID m_targetRing = kNullEntity;
    vec3     m_targetPos  = { 0.0f, -1.0f, -5.0f };
    bool     m_hasTarget  = false;

    // Shared meshes / material
    std::shared_ptr<Mesh>     m_cubeMesh;
    std::shared_ptr<Mesh>     m_groundMesh;
    std::shared_ptr<Mesh>     m_spriteMesh;
    std::shared_ptr<Mesh>     m_blobMesh;
    std::shared_ptr<Mesh>     m_sphereMesh;
    std::shared_ptr<Mesh>     m_targetRingMesh;
    std::shared_ptr<Material> m_material;

    // Entity tracking lists — parallel arrays where index matters (e.g. sprite↔blob pairing)
    std::vector<EntityID> m_cubeActors;
    std::vector<EntityID> m_spriteActors;       // 3 hero sprites
    std::vector<EntityID> m_blobActors;         // blob shadows paired by index with m_spriteActors
    std::vector<EntityID> m_bulkSpriteActors;   // 5000 GPU-instanced sprites
    std::vector<EntityID> m_meteorActors;

    std::vector<ExplosionData> m_explosions;
    std::vector<FireParticle>  m_fireParticles;

    // LCG RNG seeds kept as members so they produce deterministic sequences across frames
    uint32_t m_meteorRng   = 0xCAFEBABEu;
    uint32_t m_particleRng = 0xFEDCBA98u;
};
