#include "Game.h"
#include "DX12Context.h"
#include "Mesh.h"
#include "Material.h"
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
    m_camera.target = vec3(0.0f, 0.0f, -5.0f);

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

    // ---- World setup -------------------------------------------------------

    // Ground plane
    {
        EntityID id = m_world.CreateEntity();
        auto& xf = m_world.transforms.Add(id);
        xf.transform.SetPosition(0.0f, -1.0f, -5.0f);
        xf.transform.SetScale(100.0f, 1.0f, 100.0f);
        auto& rnd = m_world.renders.Add(id);
        rnd.mesh     = m_groundMesh;
        rnd.material = m_material;
        rnd.tint     = vec4(0.95f, 0.95f, 0.95f, 1.0f);
    }

    // Targeting ring gizmo
    {
        m_targetRing = m_world.CreateEntity();
        m_world.transforms.Add(m_targetRing);
        auto& rnd = m_world.renders.Add(m_targetRing);
        rnd.mesh                  = m_targetRingMesh;
        rnd.material              = m_material;
        rnd.castsProjectedShadow  = false;
        rnd.isUnlit               = true;
        rnd.visible               = false;
    }

    // Cubes
    m_cubeActors.reserve(3);
    for (int i = 0; i < 3; ++i)
    {
        EntityID id = m_world.CreateEntity();
        auto& xf = m_world.transforms.Add(id);
        xf.transform.SetPosition(-2.0f + i * 2.0f, 0.0f, -5.0f);
        xf.transform.SetScale(0.75f + i * 0.2f, 0.75f + i * 0.2f, 0.75f + i * 0.2f);
        xf.transform.SetRotation(QuatRotationAxis(vec3(0.0f, 1.0f, 0.0f), i * 0.35f));
        auto& rnd = m_world.renders.Add(id);
        rnd.mesh     = m_cubeMesh;
        rnd.material = m_material;
        rnd.tint     = vec4(0.2f + i * 0.3f,
                            0.2f + (i % 2) * 0.5f,
                            0.2f + ((i + 1) % 2) * 0.5f,
                            1.0f);
        m_cubeActors.push_back(id);
    }

    // Sprite actors — base Y -0.15 so bottom just touches floor at lowest hover
    m_spriteActors.reserve(3);
    for (int i = 0; i < 3; ++i)
    {
        EntityID id = m_world.CreateEntity();
        auto& xf = m_world.transforms.Add(id);
        xf.transform.SetPosition(-2.8f + i * 2.8f, -0.15f, -3.4f);
        xf.transform.SetScale(1.1f, 1.5f, 1.0f);
        auto& rnd = m_world.renders.Add(id);
        rnd.mesh                  = m_spriteMesh;
        rnd.material              = m_material;
        rnd.isBillboard           = true;
        rnd.castsProjectedShadow  = true;
        rnd.usesSpriteTexture     = true;
        m_world.sprites.Add(id); // SpriteComp for uvRect / hoverPhase / animation
        m_spriteActors.push_back(id);
    }

    // Sprite atlas animation sets
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
    for (int i = 0; i < 3; ++i)
        m_world.sprites.Get(m_spriteActors[i])->animFrames = kAnimSets[i];

    // Blob circle shadows — one per hero sprite, just above the floor
    m_blobActors.reserve(3);
    for (int i = 0; i < 3; ++i)
    {
        EntityID id = m_world.CreateEntity();
        auto& xf = m_world.transforms.Add(id);
        xf.transform.SetPosition(-2.8f + i * 2.8f, -0.999f, -3.4f);
        xf.transform.SetScale(0.7f, 1.0f, 0.7f);
        auto& rnd = m_world.renders.Add(id);
        rnd.mesh                  = m_blobMesh;
        rnd.material              = m_material;
        rnd.isBlobShadow          = true;
        rnd.castsProjectedShadow  = false;
        rnd.tint                  = vec4(0.0f, 0.0f, 0.0f, 0.75f);
        m_blobActors.push_back(id);
    }

    // 5000 bulk sprites scattered across the floor (GPU-instanced)
    static constexpr int kBulkCount = 5000;
    m_bulkSpriteActors.reserve(kBulkCount);

    uint32_t rng = 0xDEADBEEFu;
    auto Rng = [&rng]() -> float {
        rng = rng * 1664525u + 1013904223u;
        return static_cast<float>(rng >> 16) / 65535.0f;
    };

    for (int i = 0; i < kBulkCount; ++i)
    {
        const float x   = (Rng() - 0.5f) * 90.0f;
        const float z   = (Rng() - 0.5f) * 90.0f - 5.0f;
        const int   row = static_cast<int>(Rng() * 2.99f);

        EntityID id = m_world.CreateEntity();
        auto& xf = m_world.transforms.Add(id);
        xf.transform.SetPosition(x, -0.15f, z);
        xf.transform.SetScale(1.1f, 1.5f, 1.0f);
        auto& rnd = m_world.renders.Add(id);
        rnd.mesh                  = m_spriteMesh;
        rnd.material              = m_material;
        rnd.isBillboard           = true;
        rnd.castsProjectedShadow  = false;
        rnd.usesSpriteTexture     = true;
        rnd.isInstanced           = true;
        rnd.tint = vec4(0.88f + Rng() * 0.24f,
                        0.88f + Rng() * 0.24f,
                        0.88f + Rng() * 0.24f,
                        1.0f);
        auto& spr = m_world.sprites.Add(id);
        spr.animFrames = kAnimSets[row];
        spr.animTimer  = Rng() * 4.0f;
        spr.hoverPhase = static_cast<float>(i) * 0.13f;
        m_bulkSpriteActors.push_back(id);
    }

    return true;
}

void Game::Update(float dt, const InputState& input)
{
    m_time += dt;
    const float t = m_time;

    m_world.time = m_time;

    auto PRng = [this]() -> float {
        m_particleRng = m_particleRng * 1664525u + 1013904223u;
        return static_cast<float>(m_particleRng >> 16) / 65535.0f;
    };

    // ---- Camera ------------------------------------------------------------

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

        m_camAzimuth   = ct * 0.14f;
        m_camElevation = 0.08f + 0.55f * (0.5f + 0.5f * sinf(ct * 0.23f));
        m_camRadius    = 7.0f  + 14.0f * (0.5f + 0.5f * sinf(ct * 0.17f));

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
        if (input.middleMouseHeld)
        {
            const float panSpeed     = m_camRadius * 0.004f;
            const vec3  camRight     = { -sinf(m_camAzimuth), 0.0f,  cosf(m_camAzimuth) };
            const vec3  camFwdGround = { -cosf(m_camAzimuth), 0.0f, -sinf(m_camAzimuth) };
            m_camera.target.x -= camRight.x     * static_cast<float>(input.mouseDeltaX) * panSpeed;
            m_camera.target.z -= camRight.z     * static_cast<float>(input.mouseDeltaX) * panSpeed;
            m_camera.target.x -= camFwdGround.x * static_cast<float>(input.mouseDeltaY) * panSpeed;
            m_camera.target.z -= camFwdGround.z * static_cast<float>(input.mouseDeltaY) * panSpeed;
        }
        m_camRadius -= static_cast<float>(input.scrollDelta) * kZoomSensitivity;
        m_camRadius  = std::max(kMinRadius, std::min(kMaxRadius, m_camRadius));
    }

    const float cosEl = cosf(m_camElevation);
    const float sinEl = sinf(m_camElevation);
    m_camera.eye = vec3(
        m_camera.target.x + cosf(m_camAzimuth) * cosEl * m_camRadius,
        m_camera.target.y + sinEl * m_camRadius,
        m_camera.target.z + sinf(m_camAzimuth) * cosEl * m_camRadius);

    // ---- Mouse targeting ---------------------------------------------------

    if (input.screenW > 0 && input.screenH > 0)
    {
        const float aspect  = static_cast<float>(input.screenW) / static_cast<float>(input.screenH);
        const mat4  viewMat = MatrixLookAtRH(m_camera.eye, m_camera.target, vec3(0, 1, 0));
        const mat4  projMat = MatrixPerspectiveRH(m_camera.fovY, aspect, m_camera.nearZ, m_camera.farZ);

        const float ndcX = (static_cast<float>(input.mouseAbsX) / static_cast<float>(input.screenW)) * 2.0f - 1.0f;
        const float ndcY = 1.0f - (static_cast<float>(input.mouseAbsY) / static_cast<float>(input.screenH)) * 2.0f;

        const vec3 camRight = { viewMat.m[0], viewMat.m[4], viewMat.m[8]  };
        const vec3 camUp    = { viewMat.m[1], viewMat.m[5], viewMat.m[9]  };
        const vec3 camBack  = { viewMat.m[2], viewMat.m[6], viewMat.m[10] };

        const float vx = ndcX / projMat.m[0];
        const float vy = ndcY / projMat.m[5];
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

    // ---- Targeting ring gizmo ---------------------------------------------

    constexpr float kTargetRadius = 2.5f;
    if (m_targetRing != kNullEntity)
    {
        TransformComp* ringXf  = m_world.transforms.Get(m_targetRing);
        RenderComp*    ringRnd = m_world.renders.Get(m_targetRing);
        if (ringRnd) ringRnd->visible = m_hasTarget;
        if (m_hasTarget && ringXf && ringRnd)
        {
            const float pulse = 0.65f + 0.35f * sinf(t * 5.0f);
            ringXf->transform.SetPosition(m_targetPos.x, -0.97f, m_targetPos.z);
            ringXf->transform.SetScale(kTargetRadius, 1.0f, kTargetRadius);
            ringRnd->tint = vec4(1.0f, pulse, 0.05f, 1.0f);
        }
    }

    // ---- Cube animation ----------------------------------------------------

    for (size_t i = 0; i < m_cubeActors.size(); ++i)
    {
        TransformComp* xf = m_world.transforms.Get(m_cubeActors[i]);
        if (!xf) continue;
        const float phase = static_cast<float>(i) * 0.9f;
        xf->transform.SetPosition(
            -2.0f + static_cast<float>(i) * 2.0f,
            sinf(t * 1.6f + phase) * 0.35f,
            -5.0f);
        xf->transform.SetRotation(
            QuatRotationAxis(vec3(0.0f, 1.0f, 0.0f),
                             t * (0.7f + static_cast<float>(i) * 0.35f) + phase));
    }

    // ---- Sprite hover animation (hero sprites, skip if burning) -----------

    for (size_t i = 0; i < m_spriteActors.size(); ++i)
    {
        EntityID id = m_spriteActors[i];
        if (m_world.burning.Has(id)) continue;
        TransformComp* xf = m_world.transforms.Get(id);
        if (!xf) continue;
        const float phase = static_cast<float>(i) * 0.55f + 1.1f;
        xf->transform.SetPosition(
            -2.8f + static_cast<float>(i) * 2.8f,
            -0.15f + sinf(t * 1.45f + phase) * 0.10f,
            -3.4f + cosf(t * 0.70f + phase) * 0.20f);
    }

    // ---- Blob shadow — track sprite XZ, scale/fade with hover height ------

    for (size_t i = 0; i < m_blobActors.size() && i < m_spriteActors.size(); ++i)
    {
        TransformComp* sprXf  = m_world.transforms.Get(m_spriteActors[i]);
        TransformComp* blobXf = m_world.transforms.Get(m_blobActors[i]);
        RenderComp*    blobRnd = m_world.renders.Get(m_blobActors[i]);
        if (!sprXf || !blobXf || !blobRnd) continue;

        const float spriteBottomY    = sprXf->transform.position.y - 0.75f;
        const float heightAboveFloor = spriteBottomY - (-1.0f);
        const float blobAlpha = std::max(0.15f, 0.75f - heightAboveFloor * 2.0f);
        const float blobScale = 0.7f + heightAboveFloor * 2.0f;

        blobXf->transform.SetPosition(sprXf->transform.position.x, -0.999f, sprXf->transform.position.z);
        blobXf->transform.SetScale(blobScale, 1.0f, blobScale);
        blobRnd->tint.w = blobAlpha;
    }

    // ---- Sprite frame animation --------------------------------------------

    auto TickAnim = [&](EntityID id) {
        SpriteComp* spr = m_world.sprites.Get(id);
        if (!spr || spr->animFrames.empty()) return;
        spr->animTimer += dt;
        const int frame = static_cast<int>(spr->animTimer * spr->animSpeed)
                          % static_cast<int>(spr->animFrames.size());
        spr->uvRect = spr->animFrames[frame];
    };
    for (EntityID id : m_spriteActors)    TickAnim(id);
    for (EntityID id : m_bulkSpriteActors) TickAnim(id);

    // ---- Meteor spawn on left-click ----------------------------------------

    if (input.leftMouseClick && m_hasTarget && m_sphereMesh)
    {
        auto MRng = [this]() -> float {
            m_meteorRng = m_meteorRng * 1664525u + 1013904223u;
            return static_cast<float>(m_meteorRng >> 16) / 65535.0f;
        };

        const int count = 3 + static_cast<int>(MRng() * 3.0f);
        for (int i = 0; i < count; ++i)
        {
            const float dx   = (MRng() - 0.5f) * kTargetRadius * 2.0f;
            const float dz   = (MRng() - 0.5f) * kTargetRadius * 2.0f;
            const float size = 0.55f + MRng() * 0.55f;

            EntityID id = m_world.CreateEntity();
            auto& xf = m_world.transforms.Add(id);
            xf.transform.SetPosition(m_targetPos.x + dx, 22.0f, m_targetPos.z + dz);
            xf.transform.SetScale(size, size, size);
            auto& rnd = m_world.renders.Add(id);
            rnd.mesh                  = m_sphereMesh;
            rnd.material              = m_material;
            rnd.castsProjectedShadow  = true;
            rnd.tint = vec4(1.0f, 0.12f + MRng() * 0.18f, 0.02f, 1.0f);
            m_world.physics.Add(id, PhysicsComp{
                vec3((MRng() - 0.5f) * 2.5f, -8.0f, (MRng() - 0.5f) * 2.5f) });
            m_meteorActors.push_back(id);
        }
    }

    // ---- Meteor physics + fire trail ---------------------------------------

    constexpr float kMeteorGravity = 20.0f;
    for (EntityID id : m_meteorActors)
    {
        PhysicsComp*   phys = m_world.physics.Get(id);
        TransformComp* xf   = m_world.transforms.Get(id);
        if (!phys || !xf) continue;
        phys->velocity.y -= kMeteorGravity * dt;
        xf->transform.position.x += phys->velocity.x * dt;
        xf->transform.position.y += phys->velocity.y * dt;
        xf->transform.position.z += phys->velocity.z * dt;

        // Fire trail particles
        const vec3& mp         = xf->transform.position;
        const float meteorSize = xf->transform.scale.x;
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

    // ---- Meteor impact detection + explosion spawn -------------------------

    std::vector<EntityID> meteorToDestroy;
    for (EntityID id : m_meteorActors)
    {
        TransformComp* xf = m_world.transforms.Get(id);
        if (xf && xf->transform.position.y < -1.0f)
            meteorToDestroy.push_back(id);
    }

    if (!meteorToDestroy.empty())
    {
        for (EntityID id : meteorToDestroy)
        {
            TransformComp* xf = m_world.transforms.Get(id);
            if (!xf) continue;
            const float cx        = xf->transform.position.x;
            const float cz        = xf->transform.position.z;
            const float maxRadius = 1.8f + xf->transform.scale.x * 2.0f;

            EntityID expl = m_world.CreateEntity();
            auto& exXf = m_world.transforms.Add(expl);
            exXf.transform.SetPosition(cx, -1.0f, cz);
            exXf.transform.SetScale(0.01f, 0.01f, 0.01f);
            auto& exRnd = m_world.renders.Add(expl);
            exRnd.mesh                  = m_sphereMesh;
            exRnd.material              = m_material;
            exRnd.castsProjectedShadow  = false;
            exRnd.tint                  = vec4(1.0f, 0.6f, 0.0f, 1.0f);
            m_explosions.push_back({ expl, vec3(cx, -1.0f, cz), 0.0f, 1.0f, maxRadius });
        }

        m_meteorActors.erase(
            std::remove_if(m_meteorActors.begin(), m_meteorActors.end(),
                [&meteorToDestroy](EntityID id) {
                    return std::find(meteorToDestroy.begin(), meteorToDestroy.end(), id) != meteorToDestroy.end();
                }),
            m_meteorActors.end());

        for (EntityID id : meteorToDestroy)
            m_world.DestroyEntity(id);
    }

    // ---- Re-enable cubes each frame (transient hide during explosions) -----

    for (EntityID id : m_cubeActors)
    {
        RenderComp* rnd = m_world.renders.Get(id);
        if (rnd) rnd->visible = true;
    }

    // ---- Explosion update --------------------------------------------------

    for (auto& expl : m_explosions)
    {
        expl.age += dt;
        const float progress = std::min(expl.age / expl.maxAge, 1.0f);
        const float easedT   = 1.0f - (1.0f - progress) * (1.0f - progress);
        const float radius   = expl.maxRadius * easedT;
        const float diameter = radius * 2.0f;

        TransformComp* exXf  = m_world.transforms.Get(expl.sphere);
        RenderComp*    exRnd = m_world.renders.Get(expl.sphere);
        if (exXf)  exXf->transform.SetScale(diameter, diameter, diameter);
        if (exRnd) exRnd->tint = vec4(1.0f, 0.6f * (1.0f - progress), 0.0f, 1.0f);

        const float r2       = radius * radius;
        const float scorchR2 = (radius * 1.8f) * (radius * 1.8f);

        auto disableIfInside = [&](EntityID id) {
            TransformComp* xf  = m_world.transforms.Get(id);
            RenderComp*    rnd = m_world.renders.Get(id);
            if (!xf || !rnd || !rnd->visible) return;
            const vec3& p  = xf->transform.position;
            const float dx = p.x - expl.center.x;
            const float dy = p.y - expl.center.y;
            const float dz = p.z - expl.center.z;
            if (dx*dx + dy*dy + dz*dz < r2) rnd->visible = false;
        };
        for (EntityID id : m_cubeActors)       disableIfInside(id);
        for (EntityID id : m_spriteActors)     disableIfInside(id);
        for (EntityID id : m_bulkSpriteActors) disableIfInside(id);

        auto startBurning = [&](EntityID id) {
            if (m_world.burning.Has(id)) return;
            TransformComp* xf  = m_world.transforms.Get(id);
            RenderComp*    rnd = m_world.renders.Get(id);
            if (!xf || !rnd || !rnd->visible) return;
            const vec3& p  = xf->transform.position;
            const float dx = p.x - expl.center.x;
            const float dy = p.y - expl.center.y;
            const float dz = p.z - expl.center.z;
            const float d2 = dx*dx + dy*dy + dz*dz;
            if (d2 > r2 && d2 < scorchR2)
            {
                const float dist = sqrtf(d2);
                float nx = 0.0f, nz = 1.0f;
                if (dist > 0.001f) { nx = dx / dist; nz = dz / dist; }
                const float hSpeed = 2.5f + PRng() * 4.0f;
                m_world.burning.Add(id, BurningComp{ 0.0f, 1.2f + PRng() * 0.8f, rnd->tint });
                m_world.physics.Add(id, PhysicsComp{
                    vec3(nx * hSpeed, 3.5f + PRng() * 4.5f, nz * hSpeed) });
            }
        };
        for (EntityID id : m_spriteActors)     startBurning(id);
        for (EntityID id : m_bulkSpriteActors) startBurning(id);
    }

    // ---- Blob shadow tracks sprite visibility -----------------------------

    for (size_t i = 0; i < m_spriteActors.size() && i < m_blobActors.size(); ++i)
    {
        RenderComp* sprRnd  = m_world.renders.Get(m_spriteActors[i]);
        RenderComp* blobRnd = m_world.renders.Get(m_blobActors[i]);
        if (sprRnd && !sprRnd->visible && blobRnd)
            blobRnd->visible = false;
    }

    // ---- Burning sprite update (BurningComp pool is the source of truth) --

    constexpr float kRagdollGravity = 14.0f;
    constexpr float kGroundY        = -1.0f;

    std::vector<EntityID> burnDone;
    const size_t burnCount = m_world.burning.Size(); // snapshot before loop
    for (size_t i = 0; i < burnCount; ++i)
    {
        EntityID    id = m_world.burning.IdAt(i);
        BurningComp& bc = m_world.burning.DataAt(i);
        RenderComp*    rnd = m_world.renders.Get(id);

        if (!rnd || !rnd->visible) { bc.age = bc.duration; }
        else
        {
            bc.age += dt;

            // Ragdoll physics
            PhysicsComp*   phys = m_world.physics.Get(id);
            TransformComp* xf   = m_world.transforms.Get(id);
            if (phys && xf)
            {
                phys->velocity.y -= kRagdollGravity * dt;
                vec3& pos = xf->transform.position;
                pos.x += phys->velocity.x * dt;
                pos.y += phys->velocity.y * dt;
                pos.z += phys->velocity.z * dt;
                if (pos.y < kGroundY)
                {
                    pos.y           = kGroundY;
                    phys->velocity.y  = -phys->velocity.y * 0.28f;
                    phys->velocity.x *= 0.55f;
                    phys->velocity.z *= 0.55f;
                }
            }

            // Tint shift from original colour to fire colour
            const float bt = bc.age / bc.duration;
            if (bt < 0.45f) {
                const float u = bt / 0.45f;
                rnd->tint = vec4(
                    bc.origTint.x + (1.0f - bc.origTint.x) * u,
                    bc.origTint.y * (1.0f - u) + 0.25f * u,
                    bc.origTint.z * (1.0f - u),
                    1.0f);
            } else {
                const float u = (bt - 0.45f) / 0.55f;
                rnd->tint = vec4(1.0f - u * 0.85f, 0.25f * (1.0f - u), 0.0f, 1.0f);
            }

            // Particle emission
            if (PRng() < 0.65f && xf)
            {
                const vec3& sp = xf->transform.position;
                m_fireParticles.push_back({
                    vec3(sp.x + (PRng()-0.5f)*0.4f, sp.y + PRng()*0.5f, sp.z + (PRng()-0.5f)*0.4f),
                    vec3((PRng()-0.5f)*0.5f, 0.9f + PRng()*1.4f, (PRng()-0.5f)*0.5f),
                    0.0f,
                    0.35f + PRng() * 0.2f,
                    0.05f + PRng() * 0.05f
                });
            }

            // Kill on burnout
            if (bc.age >= bc.duration)
            {
                rnd->visible = false;
                // Sync blob shadow with its sprite
                for (size_t si = 0; si < m_spriteActors.size() && si < m_blobActors.size(); ++si)
                {
                    if (m_spriteActors[si] == id)
                    {
                        RenderComp* blobRnd = m_world.renders.Get(m_blobActors[si]);
                        if (blobRnd) blobRnd->visible = false;
                    }
                }
                burnDone.push_back(id);
            }
        }
    }
    for (EntityID id : burnDone)
    {
        m_world.burning.Remove(id);
        m_world.physics.Remove(id);
    }

    // ---- Fire particle physics --------------------------------------------

    for (auto& fp : m_fireParticles)
    {
        fp.vel.y  = std::max(fp.vel.y - 2.5f * dt, 0.15f);
        fp.pos.x += fp.vel.x * dt;
        fp.pos.y += fp.vel.y * dt;
        fp.pos.z += fp.vel.z * dt;
        fp.age   += dt;
    }
    m_fireParticles.erase(
        std::remove_if(m_fireParticles.begin(), m_fireParticles.end(),
            [](const FireParticle& fp) { return fp.age >= fp.maxAge; }),
        m_fireParticles.end());

    // ---- Shockwave data (read by DX12Context distort pass) ----------------

    {
        constexpr float kShockwaveDuration = 0.65f;
        auto& waves = m_world.shockwaves;
        waves.clear();
        for (const auto& expl : m_explosions)
            if (expl.age < kShockwaveDuration)
                waves.push_back({ expl.center.x, expl.center.y, expl.center.z,
                                  expl.age, kShockwaveDuration });
    }

    // ---- Scene particle list for the renderer -----------------------------

    auto& sceneParts = m_world.particles;
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

    // ---- Finished explosion cleanup ----------------------------------------

    std::vector<EntityID> explToDestroy;
    for (const auto& expl : m_explosions)
        if (expl.age >= expl.maxAge)
            explToDestroy.push_back(expl.sphere);

    if (!explToDestroy.empty())
    {
        m_explosions.erase(
            std::remove_if(m_explosions.begin(), m_explosions.end(),
                [](const ExplosionData& e) { return e.age >= e.maxAge; }),
            m_explosions.end());
        for (EntityID id : explToDestroy)
            m_world.DestroyEntity(id);
    }
}
