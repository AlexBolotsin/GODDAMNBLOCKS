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

    std::shared_ptr<Mesh> CreateTargetRingMesh(ID3D12Device* device)
    {
        constexpr int   kSegments  = 48;
        constexpr float kInnerR    = 0.82f;
        constexpr float kOuterR    = 1.00f;
        constexpr float kPi        = 3.14159265f;

        std::vector<Vertex>   vertices;
        std::vector<uint32_t> indices;
        vertices.reserve(kSegments * 4);
        indices.reserve(kSegments * 6);

        for (int i = 0; i < kSegments; ++i)
        {
            const float a0 = 2.0f * kPi * static_cast<float>(i)     / kSegments;
            const float a1 = 2.0f * kPi * static_cast<float>(i + 1) / kSegments;
            const float c0 = cosf(a0), s0 = sinf(a0);
            const float c1 = cosf(a1), s1 = sinf(a1);

            const uint32_t base = static_cast<uint32_t>(vertices.size());
            vertices.push_back({ vec3(c0 * kInnerR, 0.0f, s0 * kInnerR), vec3(0,1,0), vec4(1,1,1,1), vec2(0,0) });
            vertices.push_back({ vec3(c0 * kOuterR, 0.0f, s0 * kOuterR), vec3(0,1,0), vec4(1,1,1,1), vec2(1,0) });
            vertices.push_back({ vec3(c1 * kInnerR, 0.0f, s1 * kInnerR), vec3(0,1,0), vec4(1,1,1,1), vec2(0,1) });
            vertices.push_back({ vec3(c1 * kOuterR, 0.0f, s1 * kOuterR), vec3(0,1,0), vec4(1,1,1,1), vec2(1,1) });

            indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 1);
            indices.push_back(base + 1); indices.push_back(base + 2); indices.push_back(base + 3);
        }

        auto mesh = std::make_shared<Mesh>();
        return mesh->Init(device, vertices, indices) ? mesh : nullptr;
    }

    std::shared_ptr<Mesh> CreateSphereMesh(ID3D12Device* device)
    {
        constexpr int   stacks = 12;
        constexpr int   slices = 16;
        constexpr float radius = 0.5f;
        constexpr float kPi    = 3.14159265f;

        std::vector<Vertex>   vertices;
        std::vector<uint32_t> indices;

        for (int i = 0; i <= stacks; ++i)
        {
            const float phi    = kPi * (static_cast<float>(i) / stacks - 0.5f);
            const float cosPhi = cosf(phi);
            const float sinPhi = sinf(phi);
            for (int j = 0; j <= slices; ++j)
            {
                const float theta = 2.0f * kPi * static_cast<float>(j) / slices;
                const float nx    = cosPhi * cosf(theta);
                const float ny    = sinPhi;
                const float nz    = cosPhi * sinf(theta);
                vertices.push_back({
                    vec3(nx * radius, ny * radius, nz * radius),
                    vec3(nx, ny, nz),
                    vec4(1, 1, 1, 1),
                    vec2(static_cast<float>(j) / slices, static_cast<float>(i) / stacks)
                });
            }
        }

        for (int i = 0; i < stacks; ++i)
        {
            for (int j = 0; j < slices; ++j)
            {
                const uint32_t a = static_cast<uint32_t>(i * (slices + 1) + j);
                const uint32_t b = a + static_cast<uint32_t>(slices + 1);
                indices.push_back(a);     indices.push_back(b);     indices.push_back(a + 1);
                indices.push_back(a + 1); indices.push_back(b);     indices.push_back(b + 1);
            }
        }

        auto mesh = std::make_shared<Mesh>();
        return mesh->Init(device, vertices, indices) ? mesh : nullptr;
    }

    // Flat horizontal quad in XZ plane with full UVs — used for blob circle shadows on the floor
    std::shared_ptr<Mesh> CreateBlobShadowMesh(ID3D12Device* device)
    {
        std::vector<Vertex> vertices =
        {
            { vec3(-0.5f, 0.0f, -0.5f), vec3(0, 1, 0), vec4(1, 1, 1, 1), vec2(0.0f, 0.0f) },
            { vec3( 0.5f, 0.0f, -0.5f), vec3(0, 1, 0), vec4(1, 1, 1, 1), vec2(1.0f, 0.0f) },
            { vec3( 0.5f, 0.0f,  0.5f), vec3(0, 1, 0), vec4(1, 1, 1, 1), vec2(1.0f, 1.0f) },
            { vec3(-0.5f, 0.0f,  0.5f), vec3(0, 1, 0), vec4(1, 1, 1, 1), vec2(0.0f, 1.0f) },
        };

        std::vector<uint32_t> indices = { 0, 2, 1, 0, 3, 2 };

        auto mesh = std::make_shared<Mesh>();
        return mesh->Init(device, vertices, indices) ? mesh : nullptr;
    }
}

bool Game::Init(DX12Context& dx12, const wchar_t* shaderPath, const wchar_t* spriteSheetPath)
{
    m_cubeMesh       = CreateCubeMesh(dx12.GetDevice());
    m_groundMesh     = CreateGroundPlaneMesh(dx12.GetDevice());
    m_spriteMesh     = CreateSpriteQuadMesh(dx12.GetDevice());
    m_blobMesh       = CreateBlobShadowMesh(dx12.GetDevice());
    m_sphereMesh     = CreateSphereMesh(dx12.GetDevice());
    m_targetRingMesh = CreateTargetRingMesh(dx12.GetDevice());

    if (!m_cubeMesh || !m_groundMesh || !m_spriteMesh || !m_blobMesh || !m_sphereMesh || !m_targetRingMesh)
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
        ground.transform.SetScale(100.0f, 1.0f, 100.0f);
        ground.tint = vec4(0.95f, 0.95f, 0.95f, 1.0f);
    }

    // Targeting ring gizmo — position/visibility updated every frame in Update
    {
        Entity& ring = m_scene.CreateEntity();
        ring.mesh                 = m_targetRingMesh;
        ring.material             = m_material;
        ring.castsProjectedShadow = false;
        ring.enabled              = false;
        m_targetRing              = &ring;
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

    // Sprite actors — base Y -0.15 so bottom just touches floor at lowest hover point
    m_spriteActors.reserve(3);
    for (int i = 0; i < 3; ++i)
    {
        Entity& sprite = m_scene.CreateEntity();
        sprite.mesh = m_spriteMesh;
        sprite.material = m_material;
        sprite.isBillboardActor = true;
        sprite.castsProjectedShadow = true;
        sprite.usesSpriteTexture = true;
        sprite.transform.SetPosition(-2.8f + i * 2.8f, -0.15f, -3.4f);
        sprite.transform.SetScale(1.1f, 1.5f, 1.0f);
        sprite.tint = vec4(1.0f, 1.0f, 1.0f, 1.0f);
        m_spriteActors.push_back(&sprite);
    }

    // Blob circle shadows — one flat quad per sprite, just above the floor
    m_blobActors.reserve(3);
    for (int i = 0; i < 3; ++i)
    {
        Entity& blob = m_scene.CreateEntity();
        blob.mesh = m_blobMesh;
        blob.material = m_material;
        blob.isBlobShadow = true;
        blob.castsProjectedShadow = false;
        blob.transform.SetPosition(-2.8f + i * 2.8f, -0.999f, -3.4f);
        blob.transform.SetScale(0.7f, 1.0f, 0.7f);
        blob.tint = vec4(0.0f, 0.0f, 0.0f, 0.75f);
        m_blobActors.push_back(&blob);
    }

    const std::vector<vec4> kAnimSets[3] =
    {
        {   // row_00
            MakeAtlasRect(  2.0f,  8.0f, 18.0f, 23.0f),
            MakeAtlasRect( 24.0f,  7.0f, 19.0f, 24.0f),
            MakeAtlasRect( 47.0f,  7.0f, 19.0f, 24.0f),
            MakeAtlasRect( 70.0f,  8.0f, 18.0f, 23.0f),
            MakeAtlasRect( 92.0f,  8.0f, 19.0f, 23.0f),
            MakeAtlasRect(114.0f,  7.0f, 19.0f, 24.0f),
            MakeAtlasRect(138.0f,  7.0f, 17.0f, 24.0f),
            MakeAtlasRect(160.0f, 11.0f, 19.0f, 20.0f),
            MakeAtlasRect(209.0f,  7.0f, 18.0f, 24.0f),
        },
        {   // row_02
            MakeAtlasRect(  3.0f, 36.0f, 18.0f, 23.0f),
            MakeAtlasRect( 25.0f, 35.0f, 18.0f, 24.0f),
            MakeAtlasRect( 47.0f, 35.0f, 19.0f, 24.0f),
            MakeAtlasRect( 70.0f, 36.0f, 18.0f, 23.0f),
            MakeAtlasRect( 93.0f, 36.0f, 18.0f, 23.0f),
            MakeAtlasRect(114.0f, 35.0f, 19.0f, 24.0f),
            MakeAtlasRect(139.0f, 36.0f, 17.0f, 23.0f),
            MakeAtlasRect(160.0f, 40.0f, 19.0f, 19.0f),
            MakeAtlasRect(210.0f, 35.0f, 17.0f, 24.0f),
        },
        {   // row_04
            MakeAtlasRect(  2.0f, 64.0f, 18.0f, 24.0f),
            MakeAtlasRect( 25.0f, 64.0f, 18.0f, 24.0f),
            MakeAtlasRect( 47.0f, 64.0f, 19.0f, 24.0f),
            MakeAtlasRect( 70.0f, 64.0f, 18.0f, 24.0f),
            MakeAtlasRect( 92.0f, 64.0f, 19.0f, 24.0f),
            MakeAtlasRect(114.0f, 64.0f, 19.0f, 24.0f),
            MakeAtlasRect(139.0f, 64.0f, 14.0f, 24.0f),
            MakeAtlasRect(159.0f, 69.0f, 20.0f, 19.0f),
            MakeAtlasRect(210.0f, 64.0f, 17.0f, 24.0f),
        },
    };

    m_spriteActors[0]->animFrames = kAnimSets[0];
    m_spriteActors[1]->animFrames = kAnimSets[1];
    m_spriteActors[2]->animFrames = kAnimSets[2];

    // 5000 bulk sprites scattered across the floor
    static constexpr int kBulkCount = 5000;
    m_bulkSpriteActors.reserve(kBulkCount);

    // Deterministic LCG — no need for rand()/srand()
    uint32_t rng = 0xDEADBEEFu;
    auto Rng = [&rng]() -> float {
        rng = rng * 1664525u + 1013904223u;
        return static_cast<float>(rng >> 16) / 65535.0f;
    };

    for (int i = 0; i < kBulkCount; ++i)
    {
        const float x   = (Rng() - 0.5f) * 90.0f;
        const float z   = (Rng() - 0.5f) * 90.0f - 5.0f;
        const int   row = static_cast<int>(Rng() * 2.99f); // 0, 1, or 2

        Entity& sprite              = m_scene.CreateEntity();
        sprite.mesh                 = m_spriteMesh;
        sprite.material             = m_material;
        sprite.isBillboardActor     = true;
        sprite.castsProjectedShadow = false;
        sprite.usesSpriteTexture    = true;
        sprite.transform.SetPosition(x, -0.15f, z);
        sprite.transform.SetScale(1.1f, 1.5f, 1.0f);
        sprite.tint                 = vec4(1.0f, 1.0f, 1.0f, 1.0f);
        sprite.animFrames           = kAnimSets[row];
        sprite.animTimer            = Rng() * 4.0f; // stagger starting frame
        sprite.useInstancing        = true;
        sprite.tint = vec4(0.88f + Rng() * 0.24f,
                           0.88f + Rng() * 0.24f,
                           0.88f + Rng() * 0.24f,
                           1.0f);
        m_bulkSpriteActors.push_back(&sprite);
    }

    return true;
}

void Game::Update(float dt, const InputState& input)
{
    m_time += dt;
    const float t = m_time;

    auto PRng = [this]() -> float {
        m_particleRng = m_particleRng * 1664525u + 1013904223u;
        return static_cast<float>(m_particleRng >> 16) / 65535.0f;
    };

    if (input.cinematicToggled)
        m_cinematicMode = !m_cinematicMode;

    constexpr float kOrbitSensitivity = 0.005f;
    constexpr float kZoomSensitivity  = 0.8f / 120.0f;
    constexpr float kMinElevation     = 0.05f;
    constexpr float kMaxElevation     = 1.50f;
    constexpr float kMinRadius        = 1.5f;
    constexpr float kMaxRadius        = 30.0f;

    if (m_cinematicMode)
    {
        m_cinematicTime += dt;
        const float ct = m_cinematicTime;

        // Continuous slow orbit — different periods so the framing never repeats
        m_camAzimuth   = ct * 0.14f;
        m_camElevation = 0.08f + 0.55f * (0.5f + 0.5f * sinf(ct * 0.23f));
        m_camRadius    = 7.0f  + 14.0f * (0.5f + 0.5f * sinf(ct * 0.17f));

        // Target wanders across the crowd to survey different sections
        m_camera.target = vec3(
            sinf(ct * 0.09f) * 22.0f,
            0.0f,
            -5.0f + cosf(ct * 0.07f) * 22.0f);
    }
    else
    {
        if (input.rightMouseHeld)
        {
            m_camAzimuth   += static_cast<float>(input.mouseDeltaX) * kOrbitSensitivity;
            m_camElevation -= static_cast<float>(input.mouseDeltaY) * kOrbitSensitivity;
            m_camElevation  = std::max(kMinElevation, std::min(kMaxElevation, m_camElevation));
        }
        m_camRadius    -= static_cast<float>(input.scrollDelta) * kZoomSensitivity;
        m_camRadius     = std::max(kMinRadius, std::min(kMaxRadius, m_camRadius));
        m_camera.target = vec3(0.0f, 0.0f, -5.0f);
    }

    const float cosEl = cosf(m_camElevation);
    const float sinEl = sinf(m_camElevation);
    m_camera.eye = vec3(
        m_camera.target.x + cosf(m_camAzimuth) * cosEl * m_camRadius,
        m_camera.target.y + sinEl * m_camRadius,
        m_camera.target.z + sinf(m_camAzimuth) * cosEl * m_camRadius);

    // Unproject mouse cursor to ground plane (Y = -1.0) for targeting gizmo
    if (input.screenW > 0 && input.screenH > 0)
    {
        const float aspect  = static_cast<float>(input.screenW) / static_cast<float>(input.screenH);
        const mat4  viewMat = MatrixLookAtRH(m_camera.eye, m_camera.target, vec3(0, 1, 0));
        const mat4  projMat = MatrixPerspectiveRH(m_camera.fovY, aspect, m_camera.nearZ, m_camera.farZ);

        const float ndcX = (static_cast<float>(input.mouseAbsX) / static_cast<float>(input.screenW)) * 2.0f - 1.0f;
        const float ndcY = 1.0f - (static_cast<float>(input.mouseAbsY) / static_cast<float>(input.screenH)) * 2.0f;

        // Camera basis extracted from view matrix column vectors (stored across rows in m[])
        const vec3 camRight = { viewMat.m[0], viewMat.m[4], viewMat.m[8]  };
        const vec3 camUp    = { viewMat.m[1], viewMat.m[5], viewMat.m[9]  };
        const vec3 camBack  = { viewMat.m[2], viewMat.m[6], viewMat.m[10] };

        const float vx = ndcX / projMat.m[0]; // projMat.m[0] = f/aspect
        const float vy = ndcY / projMat.m[5]; // projMat.m[5] = f
        const vec3 rayDir = Vec3Normalize(vec3(
            camRight.x * vx + camUp.x * vy - camBack.x,
            camRight.y * vx + camUp.y * vy - camBack.y,
            camRight.z * vx + camUp.z * vy - camBack.z));

        constexpr float kGroundY = -1.0f;
        if (rayDir.y < -1e-4f)
        {
            const float tHit = (kGroundY - m_camera.eye.y) / rayDir.y;
            m_targetPos = vec3(
                m_camera.eye.x + rayDir.x * tHit,
                kGroundY,
                m_camera.eye.z + rayDir.z * tHit);
            m_hasTarget = true;
        }
        else
        {
            m_hasTarget = false;
        }
    }

    // Update targeting ring gizmo
    if (m_targetRing)
    {
        m_targetRing->enabled = m_hasTarget;
        if (m_hasTarget)
        {
            const float pulse = 0.65f + 0.35f * sinf(t * 5.0f);
            m_targetRing->transform.SetPosition(m_targetPos.x, -0.97f, m_targetPos.z);
            m_targetRing->transform.SetScale(2.8f, 1.0f, 2.8f);
            m_targetRing->tint = vec4(1.0f, pulse, 0.05f, 1.0f); // orange-yellow pulse
        }
    }

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

    // Sprite hover animation — base Y -0.15 so feet touch floor at lowest hover point
    for (size_t i = 0; i < m_spriteActors.size(); ++i)
    {
        Entity* sprite = m_spriteActors[i];
        if (!sprite)
            continue;

        const float phase = static_cast<float>(i) * 0.55f + 1.1f;
        sprite->transform.SetPosition(
            -2.8f + static_cast<float>(i) * 2.8f,
            -0.15f + sinf(t * 1.45f + phase) * 0.10f,
            -3.4f + cosf(t * 0.70f + phase) * 0.20f);
    }

    // Blob shadow — track sprite XZ, scale/fade with height above floor
    for (size_t i = 0; i < m_blobActors.size() && i < m_spriteActors.size(); ++i)
    {
        Entity* blob   = m_blobActors[i];
        Entity* sprite = m_spriteActors[i];
        if (!blob || !sprite)
            continue;

        // sprite scale Y = 1.5, so sprite bottom = center.y - 0.75
        const float spriteBottomY    = sprite->transform.position.y - 0.75f;
        const float heightAboveFloor = spriteBottomY - (-1.0f); // 0 when touching floor

        // blob gets darker and smaller the closer the sprite is to the floor
        const float blobAlpha = std::max(0.15f, 0.75f - heightAboveFloor * 2.0f);
        const float blobScale = 0.7f + heightAboveFloor * 2.0f;

        blob->transform.SetPosition(sprite->transform.position.x, -0.999f, sprite->transform.position.z);
        blob->transform.SetScale(blobScale, 1.0f, blobScale);
        blob->tint.w = blobAlpha;
    }

    // Sprite frame animation (original 3)
    for (Entity* sprite : m_spriteActors)
    {
        if (!sprite || sprite->animFrames.empty())
            continue;

        sprite->animTimer += dt;
        const int frameIndex = static_cast<int>(sprite->animTimer * sprite->animSpeed)
                               % static_cast<int>(sprite->animFrames.size());
        sprite->spriteUVRect = sprite->animFrames[frameIndex];
    }

    // Bulk sprite hover + frame animation
    for (size_t i = 0; i < m_bulkSpriteActors.size(); ++i)
    {
        Entity* sprite = m_bulkSpriteActors[i];
        if (!sprite || sprite->animFrames.empty())
            continue;

        const float phase = static_cast<float>(i) * 0.13f;
        sprite->transform.SetPosition(
            sprite->transform.position.x,
            -0.15f + sinf(t * 1.45f + phase) * 0.10f,
            sprite->transform.position.z);

        sprite->animTimer += dt;
        const int frameIndex = static_cast<int>(sprite->animTimer * sprite->animSpeed)
                               % static_cast<int>(sprite->animFrames.size());
        sprite->spriteUVRect = sprite->animFrames[frameIndex];
    }

    // Left-click casts a volley of meteors aimed at the targeting ring position
    if (input.leftMouseClick && m_hasTarget && m_sphereMesh)
    {
        auto MRng = [this]() -> float {
            m_meteorRng = m_meteorRng * 1664525u + 1013904223u;
            return static_cast<float>(m_meteorRng >> 16) / 65535.0f;
        };

        const int count = 3 + static_cast<int>(MRng() * 3.0f); // 3–5
        for (int i = 0; i < count; ++i)
        {
            const float dx   = (MRng() - 0.5f) * 5.0f; // scatter ±2.5 around click point
            const float dz   = (MRng() - 0.5f) * 5.0f;
            const float size = 0.55f + MRng() * 0.55f;

            Entity& meteor = m_scene.CreateEntity();
            meteor.mesh                 = m_sphereMesh;
            meteor.material             = m_material;
            meteor.castsProjectedShadow = true;
            meteor.transform.SetPosition(m_targetPos.x + dx, 22.0f, m_targetPos.z + dz);
            meteor.transform.SetScale(size, size, size);
            meteor.tint     = vec4(1.0f, 0.12f + MRng() * 0.18f, 0.02f, 1.0f);
            meteor.velocity = vec3((MRng() - 0.5f) * 2.5f, -8.0f, (MRng() - 0.5f) * 2.5f);
            m_meteorActors.push_back(&meteor);
        }
    }

    // Meteor physics — gravity + removal when below floor
    constexpr float kMeteorGravity = 20.0f;
    for (Entity* meteor : m_meteorActors)
    {
        meteor->velocity.y -= kMeteorGravity * dt;
        meteor->transform.position.x += meteor->velocity.x * dt;
        meteor->transform.position.y += meteor->velocity.y * dt;
        meteor->transform.position.z += meteor->velocity.z * dt;
    }

    // Emit fire trail particles from each falling meteor
    for (const Entity* meteor : m_meteorActors)
    {
        const vec3& mp         = meteor->transform.position;
        const float meteorSize = meteor->transform.scale.x;
        for (int k = 0; k < 3; ++k)
        {
            m_fireParticles.push_back({
                vec3(mp.x + (PRng() - 0.5f) * meteorSize * 0.5f,
                     mp.y + PRng() * meteorSize * 0.6f,
                     mp.z + (PRng() - 0.5f) * meteorSize * 0.5f),
                vec3((PRng() - 0.5f) * 0.8f,
                     0.4f + PRng() * 1.6f,
                     (PRng() - 0.5f) * 0.8f),
                0.0f,
                0.22f + PRng() * 0.18f,
                0.07f + PRng() * 0.09f + meteorSize * 0.07f
            });
        }
    }

    // Collect meteors that have hit the floor (explicit pass — do NOT rely on remove_if tail)
    std::vector<Entity*> meteorToDestroy;
    for (Entity* e : m_meteorActors)
        if (e->transform.position.y < -1.0f)
            meteorToDestroy.push_back(e);

    if (!meteorToDestroy.empty())
    {
        // Spawn an explosion at each impact site
        for (Entity* meteor : meteorToDestroy)
        {
            const float cx        = meteor->transform.position.x;
            const float cz        = meteor->transform.position.z;
            const float maxRadius = 1.8f + meteor->transform.scale.x * 2.0f;

            Entity& expl = m_scene.CreateEntity();
            expl.mesh                 = m_sphereMesh;
            expl.material             = m_material;
            expl.castsProjectedShadow = false;
            expl.transform.SetPosition(cx, -1.0f, cz);
            expl.transform.SetScale(0.01f, 0.01f, 0.01f);
            expl.tint = vec4(1.0f, 0.6f, 0.0f, 1.0f);
            m_explosions.push_back({ &expl, vec3(cx, -1.0f, cz), 0.0f, 1.0f, maxRadius });
        }

        // Remove from tracking list using the collected set as the predicate
        m_meteorActors.erase(
            std::remove_if(m_meteorActors.begin(), m_meteorActors.end(),
                [&meteorToDestroy](Entity* e) {
                    return std::find(meteorToDestroy.begin(), meteorToDestroy.end(), e) != meteorToDestroy.end();
                }),
            m_meteorActors.end());

        // Remove from scene
        auto& ents = m_scene.GetEntities();
        ents.erase(
            std::remove_if(ents.begin(), ents.end(),
                [&meteorToDestroy](const std::unique_ptr<Entity>& e) {
                    return std::find(meteorToDestroy.begin(), meteorToDestroy.end(), e.get()) != meteorToDestroy.end();
                }),
            ents.end());
    }

    // Cubes are temporarily hidden by explosions — restore each frame so the effect is transient.
    // Sprites are permanently killed — they are NOT re-enabled here.
    for (Entity* e : m_cubeActors) if (e) e->enabled = true;

    // Update each explosion: advance age, resize sphere, recolour, disable overlapping entities
    for (auto& expl : m_explosions)
    {
        expl.age += dt;
        const float progress     = std::min(expl.age / expl.maxAge, 1.0f);
        const float easedT       = 1.0f - (1.0f - progress) * (1.0f - progress); // ease-out
        const float radius       = expl.maxRadius * easedT;
        const float diameter     = radius * 2.0f;
        expl.sphere->transform.SetScale(diameter, diameter, diameter);
        expl.sphere->tint = vec4(1.0f, 0.6f * (1.0f - progress), 0.0f, 1.0f); // orange → red

        const float r2 = radius * radius;
        auto disableIfInside = [&](Entity* e) {
            if (!e) return;
            const vec3& p = e->transform.position;
            const float dx = p.x - expl.center.x;
            const float dy = p.y - expl.center.y;
            const float dz = p.z - expl.center.z;
            if (dx*dx + dy*dy + dz*dz < r2)
                e->enabled = false;
        };

        for (Entity* e : m_cubeActors)       disableIfInside(e);
        for (Entity* e : m_spriteActors)     disableIfInside(e);
        for (Entity* e : m_bulkSpriteActors) disableIfInside(e);

        // Scorch ring: sprites between kill radius and 1.8× start burning
        const float scorchR2 = (radius * 1.8f) * (radius * 1.8f);
        auto startBurning = [&](Entity* e) {
            if (!e || !e->enabled || e->isBurning) return;
            const vec3& p = e->transform.position;
            const float dx = p.x - expl.center.x;
            const float dy = p.y - expl.center.y;
            const float dz = p.z - expl.center.z;
            const float d2 = dx*dx + dy*dy + dz*dz;
            if (d2 > r2 && d2 < scorchR2) {
                e->isBurning = true;
                m_burningSprites.push_back({ e, 0.0f, 0.5f + PRng() * 0.6f, e->tint });
            }
        };
        for (Entity* e : m_spriteActors)     startBurning(e);
        for (Entity* e : m_bulkSpriteActors) startBurning(e);
    }

    // Blob shadows mirror their sprite's visibility
    for (size_t i = 0; i < m_spriteActors.size() && i < m_blobActors.size(); ++i)
        if (m_spriteActors[i] && !m_spriteActors[i]->enabled && m_blobActors[i])
            m_blobActors[i]->enabled = false;

    // Update burning sprites — shift tint orange→dark, emit particles, kill on burnout
    for (auto& bs : m_burningSprites)
    {
        if (!bs.entity->enabled) { bs.age = bs.duration; continue; } // already kill-zoned
        bs.age += dt;
        const float bt = bs.age / bs.duration;

        if (bt < 0.45f) {
            const float u = bt / 0.45f;
            bs.entity->tint = vec4(
                bs.origTint.x + (1.0f - bs.origTint.x) * u,
                bs.origTint.y * (1.0f - u) + 0.25f * u,
                bs.origTint.z * (1.0f - u),
                1.0f);
        } else {
            const float u = (bt - 0.45f) / 0.55f;
            bs.entity->tint = vec4(1.0f - u * 0.85f, 0.25f * (1.0f - u), 0.0f, 1.0f);
        }

        if (PRng() < 0.65f) {
            const vec3& sp = bs.entity->transform.position;
            m_fireParticles.push_back({
                vec3(sp.x + (PRng()-0.5f)*0.4f, sp.y + PRng()*0.5f, sp.z + (PRng()-0.5f)*0.4f),
                vec3((PRng()-0.5f)*0.5f, 0.9f + PRng()*1.4f, (PRng()-0.5f)*0.5f),
                0.0f,
                0.35f + PRng() * 0.2f,
                0.05f + PRng() * 0.05f
            });
        }

        if (bs.age >= bs.duration) {
            bs.entity->enabled = false;
            for (size_t si = 0; si < m_spriteActors.size() && si < m_blobActors.size(); ++si)
                if (m_spriteActors[si] == bs.entity && m_blobActors[si])
                    m_blobActors[si]->enabled = false;
        }
    }
    m_burningSprites.erase(
        std::remove_if(m_burningSprites.begin(), m_burningSprites.end(),
            [](const BurningSprite& bs) { return bs.age >= bs.duration; }),
        m_burningSprites.end());

    // Fire particle physics — rise with slight deceleration, age out
    for (auto& fp : m_fireParticles)
    {
        fp.vel.y   = std::max(fp.vel.y - 2.5f * dt, 0.15f);
        fp.pos.x  += fp.vel.x * dt;
        fp.pos.y  += fp.vel.y * dt;
        fp.pos.z  += fp.vel.z * dt;
        fp.age    += dt;
    }
    m_fireParticles.erase(
        std::remove_if(m_fireParticles.begin(), m_fireParticles.end(),
            [](const FireParticle& fp) { return fp.age >= fp.maxAge; }),
        m_fireParticles.end());

    // Build scene particle list for the renderer
    auto& sceneParts = m_scene.GetParticles();
    sceneParts.clear();
    sceneParts.reserve(m_fireParticles.size());
    for (const auto& fp : m_fireParticles)
    {
        const float pt    = fp.age / fp.maxAge;
        const float alpha = (1.0f - pt) * (1.0f - pt * 0.5f);
        const float g     = std::max(0.0f, 0.75f - pt * 1.1f);
        const float b     = std::max(0.0f, 0.10f - pt * 0.35f);
        const float sz    = fp.startSize * std::max(0.1f, 1.0f - pt * 0.55f);
        sceneParts.push_back({ fp.pos.x, fp.pos.y, fp.pos.z, sz, 1.0f, g, b, alpha });
    }

    // Collect finished explosions (explicit pass — do NOT rely on remove_if tail)
    std::vector<Entity*> explToDestroy;
    for (const auto& expl : m_explosions)
        if (expl.age >= expl.maxAge)
            explToDestroy.push_back(expl.sphere);

    if (!explToDestroy.empty())
    {
        m_explosions.erase(
            std::remove_if(m_explosions.begin(), m_explosions.end(),
                [](const ExplosionData& e) { return e.age >= e.maxAge; }),
            m_explosions.end());

        auto& ents = m_scene.GetEntities();
        ents.erase(
            std::remove_if(ents.begin(), ents.end(),
                [&explToDestroy](const std::unique_ptr<Entity>& e) {
                    return std::find(explToDestroy.begin(), explToDestroy.end(), e.get()) != explToDestroy.end();
                }),
            ents.end());
    }
}
