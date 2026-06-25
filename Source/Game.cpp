#include "Game.h"
#include "DX12Context.h"
#include "Mesh.h"
#include "Material.h"
#include "Entity.h"
#include "InputState.h"
#include <cmath>
#include <algorithm>

namespace
{
    constexpr float kSheetW = 293.0f;
    constexpr float kSheetH = 382.0f;

    vec4 MakeAtlasRect(float x, float y, float w, float h)
    {
        return vec4(x / kSheetW, y / kSheetH, (x + w) / kSheetW, (y + h) / kSheetH);
    }

    std::shared_ptr<Mesh> CreateCubeMesh(ID3D12Device* device)
    {
        std::vector<Vertex> vertices =
        {
            { vec3(-0.5f, -0.5f, -0.5f), vec3(0, 0, -1), vec4(1, 0, 0, 1) },
            { vec3( 0.5f, -0.5f, -0.5f), vec3(0, 0, -1), vec4(1, 0, 0, 1) },
            { vec3( 0.5f,  0.5f, -0.5f), vec3(0, 0, -1), vec4(1, 0, 0, 1) },
            { vec3(-0.5f,  0.5f, -0.5f), vec3(0, 0, -1), vec4(1, 0, 0, 1) },

            { vec3(-0.5f, -0.5f,  0.5f), vec3(0, 0, 1), vec4(0, 1, 0, 1) },
            { vec3(-0.5f,  0.5f,  0.5f), vec3(0, 0, 1), vec4(0, 1, 0, 1) },
            { vec3( 0.5f,  0.5f,  0.5f), vec3(0, 0, 1), vec4(0, 1, 0, 1) },
            { vec3( 0.5f, -0.5f,  0.5f), vec3(0, 0, 1), vec4(0, 1, 0, 1) },

            { vec3(-0.5f, 0.5f, -0.5f), vec3(0, 1, 0), vec4(0, 0, 1, 1) },
            { vec3( 0.5f, 0.5f, -0.5f), vec3(0, 1, 0), vec4(0, 0, 1, 1) },
            { vec3( 0.5f, 0.5f,  0.5f), vec3(0, 1, 0), vec4(0, 0, 1, 1) },
            { vec3(-0.5f, 0.5f,  0.5f), vec3(0, 1, 0), vec4(0, 0, 1, 1) },

            { vec3(-0.5f, -0.5f, -0.5f), vec3(0, -1, 0), vec4(1, 1, 0, 1) },
            { vec3(-0.5f, -0.5f,  0.5f), vec3(0, -1, 0), vec4(1, 1, 0, 1) },
            { vec3( 0.5f, -0.5f,  0.5f), vec3(0, -1, 0), vec4(1, 1, 0, 1) },
            { vec3( 0.5f, -0.5f, -0.5f), vec3(0, -1, 0), vec4(1, 1, 0, 1) },

            { vec3(0.5f, -0.5f, -0.5f), vec3(1, 0, 0), vec4(1, 0, 1, 1) },
            { vec3(0.5f,  0.5f, -0.5f), vec3(1, 0, 0), vec4(1, 0, 1, 1) },
            { vec3(0.5f,  0.5f,  0.5f), vec3(1, 0, 0), vec4(1, 0, 1, 1) },
            { vec3(0.5f, -0.5f,  0.5f), vec3(1, 0, 0), vec4(1, 0, 1, 1) },

            { vec3(-0.5f, -0.5f, -0.5f), vec3(-1, 0, 0), vec4(0, 1, 1, 1) },
            { vec3(-0.5f, -0.5f,  0.5f), vec3(-1, 0, 0), vec4(0, 1, 1, 1) },
            { vec3(-0.5f,  0.5f,  0.5f), vec3(-1, 0, 0), vec4(0, 1, 1, 1) },
            { vec3(-0.5f,  0.5f, -0.5f), vec3(-1, 0, 0), vec4(0, 1, 1, 1) },
        };

        std::vector<uint32_t> indices =
        {
            0, 2, 1, 0, 3, 2,
            4, 6, 5, 4, 7, 6,
            8, 10, 9, 8, 11, 10,
            12, 14, 13, 12, 15, 14,
            16, 17, 18, 16, 18, 19,
            20, 21, 22, 20, 22, 23,
        };

        auto mesh = std::make_shared<Mesh>();
        return mesh->Init(device, vertices, indices) ? mesh : nullptr;
    }

    std::shared_ptr<Mesh> CreateGroundPlaneMesh(ID3D12Device* device)
    {
        std::vector<Vertex> vertices =
        {
            { vec3(-1.0f, 0.0f, -1.0f), vec3(0, 1, 0), vec4(1, 1, 1, 1) },
            { vec3( 1.0f, 0.0f, -1.0f), vec3(0, 1, 0), vec4(1, 1, 1, 1) },
            { vec3( 1.0f, 0.0f,  1.0f), vec3(0, 1, 0), vec4(1, 1, 1, 1) },
            { vec3(-1.0f, 0.0f,  1.0f), vec3(0, 1, 0), vec4(1, 1, 1, 1) },
        };

        std::vector<uint32_t> indices = { 0, 2, 1, 0, 3, 2 };

        auto mesh = std::make_shared<Mesh>();
        return mesh->Init(device, vertices, indices) ? mesh : nullptr;
    }

    std::shared_ptr<Mesh> CreateSpriteQuadMesh(ID3D12Device* device)
    {
        std::vector<Vertex> vertices =
        {
            { vec3(-0.5f, -0.5f, 0.0f), vec3(0, 0, 1), vec4(1, 1, 1, 1), vec2(0.0f, 1.0f) },
            { vec3( 0.5f, -0.5f, 0.0f), vec3(0, 0, 1), vec4(1, 1, 1, 1), vec2(1.0f, 1.0f) },
            { vec3( 0.5f,  0.5f, 0.0f), vec3(0, 0, 1), vec4(1, 1, 1, 1), vec2(1.0f, 0.0f) },
            { vec3(-0.5f,  0.5f, 0.0f), vec3(0, 0, 1), vec4(1, 1, 1, 1), vec2(0.0f, 0.0f) },
        };

        std::vector<uint32_t> indices = { 0, 1, 2, 0, 2, 3 };

        auto mesh = std::make_shared<Mesh>();
        return mesh->Init(device, vertices, indices) ? mesh : nullptr;
    }
}

bool Game::Init(DX12Context& dx12, const wchar_t* shaderPath, const wchar_t* spriteSheetPath)
{
    m_cubeMesh   = CreateCubeMesh(dx12.GetDevice());
    m_groundMesh = CreateGroundPlaneMesh(dx12.GetDevice());
    m_spriteMesh = CreateSpriteQuadMesh(dx12.GetDevice());

    if (!m_cubeMesh || !m_groundMesh || !m_spriteMesh)
    {
        OutputDebugStringW(L"Game::Init failed to create meshes\n");
        return false;
    }

    m_material = std::make_shared<Material>();
    if (!m_material->Init(dx12.GetDevice(), dx12.GetCommandQueue(), shaderPath, spriteSheetPath, dx12.GetMsaaSampleCount()))
    {
        const Material::InitFailureStage stage = m_material->GetLastInitFailureStage();
        if (stage == Material::InitFailureStage::RootSignature)
            OutputDebugStringW(L"Game::Init material failed in CreateRootSignature\n");
        else if (stage == Material::InitFailureStage::TextureResources)
            OutputDebugStringW(L"Game::Init material failed in CreateTextureResources\n");
        else if (stage == Material::InitFailureStage::PipelineState)
            OutputDebugStringW(L"Game::Init material failed in CreatePipelineState\n");
        return false;
    }
    m_material->SetShadowMap(dx12.GetDevice(), dx12.GetShadowMap());

    // Ground plane
    {
        Entity& ground = m_scene.CreateEntity();
        ground.mesh = m_groundMesh;
        ground.material = m_material;
        ground.transform.SetPosition(0.0f, -1.0f, -5.0f);
        ground.transform.SetScale(12.0f, 1.0f, 12.0f);
        ground.tint = vec4(0.95f, 0.95f, 0.95f, 1.0f);
    }

    // Cubes
    m_cubeActors.reserve(3);
    for (int i = 0; i < 3; ++i)
    {
        Entity& entity = m_scene.CreateEntity();
        entity.mesh = m_cubeMesh;
        entity.material = m_material;
        entity.transform.SetPosition(-2.0f + i * 2.0f, 0.0f, -5.0f);
        entity.transform.SetScale(0.75f + i * 0.2f, 0.75f + i * 0.2f, 0.75f + i * 0.2f);
        entity.transform.SetRotation(QuatRotationAxis(vec3(0.0f, 1.0f, 0.0f), i * 0.35f));
        entity.tint = vec4(
            0.2f + i * 0.3f,
            0.2f + (i % 2) * 0.5f,
            0.2f + ((i + 1) % 2) * 0.5f,
            1.0f);
        m_cubeActors.push_back(&entity);
    }

    // Sprite actors
    m_spriteActors.reserve(3);
    for (int i = 0; i < 3; ++i)
    {
        Entity& sprite = m_scene.CreateEntity();
        sprite.mesh = m_spriteMesh;
        sprite.material = m_material;
        sprite.isBillboardActor = true;
        sprite.castsProjectedShadow = false;
        sprite.usesSpriteTexture = true;
        sprite.transform.SetPosition(-2.8f + i * 2.8f, 0.45f, -3.4f);
        sprite.transform.SetScale(1.1f, 1.5f, 1.0f);
        sprite.tint = vec4(1.0f, 1.0f, 1.0f, 1.0f);
        m_spriteActors.push_back(&sprite);
    }

    // row_00
    m_spriteActors[0]->animFrames = {
        MakeAtlasRect(  2.0f,  8.0f, 18.0f, 23.0f),
        MakeAtlasRect( 24.0f,  7.0f, 19.0f, 24.0f),
        MakeAtlasRect( 47.0f,  7.0f, 19.0f, 24.0f),
        MakeAtlasRect( 70.0f,  8.0f, 18.0f, 23.0f),
        MakeAtlasRect( 92.0f,  8.0f, 19.0f, 23.0f),
        MakeAtlasRect(114.0f,  7.0f, 19.0f, 24.0f),
        MakeAtlasRect(138.0f,  7.0f, 17.0f, 24.0f),
        MakeAtlasRect(160.0f, 11.0f, 19.0f, 20.0f),
        MakeAtlasRect(209.0f,  7.0f, 18.0f, 24.0f),
    };
    // row_02
    m_spriteActors[1]->animFrames = {
        MakeAtlasRect(  3.0f, 36.0f, 18.0f, 23.0f),
        MakeAtlasRect( 25.0f, 35.0f, 18.0f, 24.0f),
        MakeAtlasRect( 47.0f, 35.0f, 19.0f, 24.0f),
        MakeAtlasRect( 70.0f, 36.0f, 18.0f, 23.0f),
        MakeAtlasRect( 93.0f, 36.0f, 18.0f, 23.0f),
        MakeAtlasRect(114.0f, 35.0f, 19.0f, 24.0f),
        MakeAtlasRect(139.0f, 36.0f, 17.0f, 23.0f),
        MakeAtlasRect(160.0f, 40.0f, 19.0f, 19.0f),
        MakeAtlasRect(210.0f, 35.0f, 17.0f, 24.0f),
    };
    // row_04
    m_spriteActors[2]->animFrames = {
        MakeAtlasRect(  2.0f, 64.0f, 18.0f, 24.0f),
        MakeAtlasRect( 25.0f, 64.0f, 18.0f, 24.0f),
        MakeAtlasRect( 47.0f, 64.0f, 19.0f, 24.0f),
        MakeAtlasRect( 70.0f, 64.0f, 18.0f, 24.0f),
        MakeAtlasRect( 92.0f, 64.0f, 19.0f, 24.0f),
        MakeAtlasRect(114.0f, 64.0f, 19.0f, 24.0f),
        MakeAtlasRect(139.0f, 64.0f, 14.0f, 24.0f),
        MakeAtlasRect(159.0f, 69.0f, 20.0f, 19.0f),
        MakeAtlasRect(210.0f, 64.0f, 17.0f, 24.0f),
    };

    return true;
}

void Game::Update(float dt, const InputState& input)
{
    m_time += dt;
    const float t = m_time;

    // Orbit camera from mouse input
    constexpr float kOrbitSensitivity = 0.005f;
    constexpr float kZoomSensitivity  = 0.8f / 120.0f;
    constexpr float kMinElevation     = 0.05f;
    constexpr float kMaxElevation     = 1.50f;
    constexpr float kMinRadius        = 1.5f;
    constexpr float kMaxRadius        = 30.0f;

    m_camAzimuth   += static_cast<float>(input.mouseDeltaX) * kOrbitSensitivity;
    m_camElevation -= static_cast<float>(input.mouseDeltaY) * kOrbitSensitivity;
    m_camElevation  = std::max(kMinElevation, std::min(kMaxElevation, m_camElevation));
    m_camRadius    -= static_cast<float>(input.scrollDelta) * kZoomSensitivity;
    m_camRadius     = std::max(kMinRadius, std::min(kMaxRadius, m_camRadius));

    m_camera.target = vec3(0.0f, 0.0f, -5.0f);
    const float cosEl = cosf(m_camElevation);
    const float sinEl = sinf(m_camElevation);
    m_camera.eye = vec3(
        m_camera.target.x + cosf(m_camAzimuth) * cosEl * m_camRadius,
        m_camera.target.y + sinEl * m_camRadius,
        m_camera.target.z + sinf(m_camAzimuth) * cosEl * m_camRadius);

    // Cube animation
    for (size_t i = 0; i < m_cubeActors.size(); ++i)
    {
        Entity* entity = m_cubeActors[i];
        if (!entity)
            continue;

        const float phase = static_cast<float>(i) * 0.9f;
        entity->transform.SetPosition(
            -2.0f + static_cast<float>(i) * 2.0f,
            sinf(t * 1.6f + phase) * 0.35f,
            -5.0f);
        entity->transform.SetRotation(
            QuatRotationAxis(vec3(0.0f, 1.0f, 0.0f),
                             t * (0.7f + static_cast<float>(i) * 0.35f) + phase));
    }

    // Sprite hover animation
    for (size_t i = 0; i < m_spriteActors.size(); ++i)
    {
        Entity* sprite = m_spriteActors[i];
        if (!sprite)
            continue;

        const float phase = static_cast<float>(i) * 0.55f + 1.1f;
        sprite->transform.SetPosition(
            -2.8f + static_cast<float>(i) * 2.8f,
            0.45f + sinf(t * 1.45f + phase) * 0.18f,
            -3.4f + cosf(t * 0.70f + phase) * 0.20f);
    }

    // Sprite frame animation
    for (Entity* sprite : m_spriteActors)
    {
        if (!sprite || sprite->animFrames.empty())
            continue;

        sprite->animTimer += dt;
        const int frameIndex = static_cast<int>(sprite->animTimer * sprite->animSpeed)
                               % static_cast<int>(sprite->animFrames.size());
        sprite->spriteUVRect = sprite->animFrames[frameIndex];
    }
}
