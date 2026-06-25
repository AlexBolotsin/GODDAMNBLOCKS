#pragma once
#include "Camera.h"
#include "InputState.h"
#include "Scene.h"
#include <memory>
#include <vector>

class DX12Context;
class Mesh;
class Material;
class Entity;

class Game
{
public:
    bool Init(DX12Context& dx12, const wchar_t* shaderPath, const wchar_t* spriteSheetPath);
    void Update(float dt, const InputState& input);

    Scene&        GetScene()           { return m_scene; }
    const Camera& GetCamera() const    { return m_camera; }

private:
    Scene    m_scene;
    Camera   m_camera;
    float    m_time = 0.0f;

    float m_camAzimuth   = 0.0f;
    float m_camElevation = 0.245f;
    float m_camRadius    = 8.25f;

    std::shared_ptr<Mesh>     m_cubeMesh;
    std::shared_ptr<Mesh>     m_groundMesh;
    std::shared_ptr<Mesh>     m_spriteMesh;
    std::shared_ptr<Mesh>     m_blobMesh;
    std::shared_ptr<Material> m_material;
    std::vector<Entity*>      m_cubeActors;
    std::vector<Entity*>      m_spriteActors;
    std::vector<Entity*>      m_blobActors;
    std::vector<Entity*>      m_bulkSpriteActors;
};
