#include "GrmWindowWrapper.h"
#include "DX12Context.h"
#include "Scene.h"
#include "Mesh.h"
#include "Material.h"
#include <memory>
#include <chrono>
#include <cmath>
#include <vector>
#include <filesystem>
#include <string>

namespace
{
    std::wstring GetSpriteSheetPath()
    {
        wchar_t modulePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        std::filesystem::path path(modulePath);
        path = path.parent_path().parent_path().parent_path() / "Sprites" / "19338.png";
        return path.wstring();
    }

    vec4 MakeAtlasRect(float x, float y, float width, float height, float textureWidth, float textureHeight)
    {
        return vec4(
            x / textureWidth,
            y / textureHeight,
            (x + width) / textureWidth,
            (y + height) / textureHeight);
    }

    constexpr float SpriteSheetWidth = 293.0f;
    constexpr float SpriteSheetHeight = 382.0f;

}

// Helper: Create a simple cube mesh
std::shared_ptr<Mesh> CreateCubeMesh(ID3D12Device* device)
{
    std::vector<Vertex> vertices =
    {
        // Front face
        { vec3(-0.5f, -0.5f, -0.5f), vec3(0, 0, -1), vec4(1, 0, 0, 1) },
        { vec3(0.5f, -0.5f, -0.5f), vec3(0, 0, -1), vec4(1, 0, 0, 1) },
        { vec3(0.5f, 0.5f, -0.5f), vec3(0, 0, -1), vec4(1, 0, 0, 1) },
        { vec3(-0.5f, 0.5f, -0.5f), vec3(0, 0, -1), vec4(1, 0, 0, 1) },

        // Back face
        { vec3(-0.5f, -0.5f, 0.5f), vec3(0, 0, 1), vec4(0, 1, 0, 1) },
        { vec3(-0.5f, 0.5f, 0.5f), vec3(0, 0, 1), vec4(0, 1, 0, 1) },
        { vec3(0.5f, 0.5f, 0.5f), vec3(0, 0, 1), vec4(0, 1, 0, 1) },
        { vec3(0.5f, -0.5f, 0.5f), vec3(0, 0, 1), vec4(0, 1, 0, 1) },

        // Top face
        { vec3(-0.5f, 0.5f, -0.5f), vec3(0, 1, 0), vec4(0, 0, 1, 1) },
        { vec3(0.5f, 0.5f, -0.5f), vec3(0, 1, 0), vec4(0, 0, 1, 1) },
        { vec3(0.5f, 0.5f, 0.5f), vec3(0, 1, 0), vec4(0, 0, 1, 1) },
        { vec3(-0.5f, 0.5f, 0.5f), vec3(0, 1, 0), vec4(0, 0, 1, 1) },

        // Bottom face
        { vec3(-0.5f, -0.5f, -0.5f), vec3(0, -1, 0), vec4(1, 1, 0, 1) },
        { vec3(-0.5f, -0.5f, 0.5f), vec3(0, -1, 0), vec4(1, 1, 0, 1) },
        { vec3(0.5f, -0.5f, 0.5f), vec3(0, -1, 0), vec4(1, 1, 0, 1) },
        { vec3(0.5f, -0.5f, -0.5f), vec3(0, -1, 0), vec4(1, 1, 0, 1) },

        // Right face
        { vec3(0.5f, -0.5f, -0.5f), vec3(1, 0, 0), vec4(1, 0, 1, 1) },
        { vec3(0.5f, 0.5f, -0.5f), vec3(1, 0, 0), vec4(1, 0, 1, 1) },
        { vec3(0.5f, 0.5f, 0.5f), vec3(1, 0, 0), vec4(1, 0, 1, 1) },
        { vec3(0.5f, -0.5f, 0.5f), vec3(1, 0, 0), vec4(1, 0, 1, 1) },

        // Left face
        { vec3(-0.5f, -0.5f, -0.5f), vec3(-1, 0, 0), vec4(0, 1, 1, 1) },
        { vec3(-0.5f, -0.5f, 0.5f), vec3(-1, 0, 0), vec4(0, 1, 1, 1) },
        { vec3(-0.5f, 0.5f, 0.5f), vec3(-1, 0, 0), vec4(0, 1, 1, 1) },
        { vec3(-0.5f, 0.5f, -0.5f), vec3(-1, 0, 0), vec4(0, 1, 1, 1) },
    };

    std::vector<uint32_t> indices =
    {
        0, 2, 1, 0, 3, 2,       // Front
        4, 6, 5, 4, 7, 6,       // Back
        8, 10, 9, 8, 11, 10,    // Top
        12, 14, 13, 12, 15, 14, // Bottom
        16, 17, 18, 16, 18, 19, // Right
        20, 21, 22, 20, 22, 23  // Left
    };

    auto mesh = std::make_shared<Mesh>();
    if (!mesh->Init(device, vertices, indices))
        return nullptr;

    return mesh;
}

std::shared_ptr<Mesh> CreateGroundPlaneMesh(ID3D12Device* device)
{
    std::vector<Vertex> vertices =
    {
        { vec3(-1.0f, 0.0f, -1.0f), vec3(0, 1, 0), vec4(1, 1, 1, 1) },
        { vec3(1.0f, 0.0f, -1.0f),  vec3(0, 1, 0), vec4(1, 1, 1, 1) },
        { vec3(1.0f, 0.0f, 1.0f),   vec3(0, 1, 0), vec4(1, 1, 1, 1) },
        { vec3(-1.0f, 0.0f, 1.0f),  vec3(0, 1, 0), vec4(1, 1, 1, 1) },
    };

    std::vector<uint32_t> indices =
    {
        0, 2, 1,
        0, 3, 2,
    };

    auto mesh = std::make_shared<Mesh>();
    if (!mesh->Init(device, vertices, indices))
        return nullptr;

    return mesh;
}

std::shared_ptr<Mesh> CreateSpriteQuadMesh(ID3D12Device* device)
{
    std::vector<Vertex> vertices =
    {
        { vec3(-0.5f, -0.5f, 0.0f), vec3(0, 0, 1), vec4(1, 1, 1, 1), vec2(0.0f, 1.0f) },
        { vec3(0.5f, -0.5f, 0.0f),  vec3(0, 0, 1), vec4(1, 1, 1, 1), vec2(1.0f, 1.0f) },
        { vec3(0.5f, 0.5f, 0.0f),   vec3(0, 0, 1), vec4(1, 1, 1, 1), vec2(1.0f, 0.0f) },
        { vec3(-0.5f, 0.5f, 0.0f),  vec3(0, 0, 1), vec4(1, 1, 1, 1), vec2(0.0f, 0.0f) },
    };

    std::vector<uint32_t> indices =
    {
        // Front face
        0, 1, 2,
        0, 2, 3,
    };

    auto mesh = std::make_shared<Mesh>();
    if (!mesh->Init(device, vertices, indices))
        return nullptr;

    return mesh;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int /*nCmdShow*/)
{
    const HRESULT comInitHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitializeCom = SUCCEEDED(comInitHr);
    if (FAILED(comInitHr) && comInitHr != RPC_E_CHANGED_MODE)
    {
        OutputDebugStringW(L"Failed to initialize COM for sprite loading.\n");
        return -100;
    }

    // Setup window
    WindowDesc desc;
    desc.title = L"MazeGame";
    desc.width = 1280;
    desc.height = 720;
    desc.isFullScreen = false;
    desc.showCursor = false;

    GRMWindowWrapper window;

    window.OnDestroy = []()
    {
        OutputDebugStringW(L"Window destroyed\n");
    };

    if (!window.Init(hInstance, desc))
    {
        return -1;
    }

    DX12Context dx12;
    if (!dx12.Init(window.GetHWND(), window.GetWidth(), window.GetHeight()))
    {
        OutputDebugStringW(L"Failed to initialize DirectX 12.\n");
        return -1;
    }

    window.OnResize = [&dx12](uint32_t width, uint32_t height)
    {
        dx12.Resize(width, height);
    };

    // Create scene and populate with test objects
    Scene scene;

    // Create shared mesh
    auto cubeMesh = CreateCubeMesh(dx12.GetDevice());
    auto groundMesh = CreateGroundPlaneMesh(dx12.GetDevice());
    auto spriteMesh = CreateSpriteQuadMesh(dx12.GetDevice());

    if (!cubeMesh || !groundMesh || !spriteMesh)
    {
        OutputDebugStringW(L"Failed to create scene mesh.\n");
        dx12.Shutdown();
        return -1;
    }

    const std::wstring spriteSheetPath = GetSpriteSheetPath();

    auto sharedMaterial = std::make_shared<Material>();
    if (!sharedMaterial->Init(dx12.GetDevice(), dx12.GetCommandQueue(), spriteSheetPath.c_str(), dx12.GetMsaaSampleCount()))
    {
        const Material::InitFailureStage stage = sharedMaterial->GetLastInitFailureStage();
        const wchar_t* message = L"Failed to initialize shared material.";
        int exitCode = -2;

        if (stage == Material::InitFailureStage::RootSignature)
        {
            message = L"Shared material failed in CreateRootSignature.";
            exitCode = -20;
        }
        else if (stage == Material::InitFailureStage::TextureResources)
        {
            message = L"Shared material failed in CreateTextureResources.";
            exitCode = -21;
        }
        else if (stage == Material::InitFailureStage::PipelineState)
        {
            message = L"Shared material failed in CreatePipelineState.";
            exitCode = -22;
        }

        OutputDebugStringW(message);
        OutputDebugStringW(L"\n");
        dx12.Shutdown();
        return exitCode;
    }

    Entity& ground = scene.CreateEntity();
    ground.mesh = groundMesh;
    ground.material = sharedMaterial;
    ground.transform.SetPosition(0.0f, -1.0f, -5.0f);
    ground.transform.SetScale(12.0f, 1.0f, 12.0f);
    ground.tint = vec4(0.95f, 0.95f, 0.95f, 1.0f);

    std::vector<Entity*> cubeActors;
    cubeActors.reserve(3);

    // Create multiple cubes with different positions and colors
    for (int i = 0; i < 3; ++i)
    {
        Entity& entity = scene.CreateEntity();
        entity.mesh = cubeMesh;
        entity.material = sharedMaterial;
        entity.transform.SetPosition(-2.0f + i * 2.0f, 0.0f, -5.0f);
        entity.transform.SetScale(0.75f + i * 0.2f, 0.75f + i * 0.2f, 0.75f + i * 0.2f);
        entity.transform.SetRotation(QuatRotationAxis(vec3(0.0f, 1.0f, 0.0f), i * 0.35f));
        entity.tint = vec4(
            0.2f + i * 0.3f,
            0.2f + (i % 2) * 0.5f,
            0.2f + ((i + 1) % 2) * 0.5f,
            1.0f
        );
        cubeActors.push_back(&entity);
    }

    std::vector<Entity*> spriteActors;
    spriteActors.reserve(3);

    for (int i = 0; i < 3; ++i)
    {
        Entity& sprite = scene.CreateEntity();
        sprite.mesh = spriteMesh;
        sprite.material = sharedMaterial;
        sprite.isBillboardActor = true;
        sprite.castsProjectedShadow = false;
        sprite.usesSpriteTexture = true;
        sprite.transform.SetPosition(-2.8f + i * 2.8f, 0.45f, -3.4f);
        sprite.transform.SetScale(1.1f, 1.5f, 1.0f);
        sprite.tint = vec4(1.0f, 1.0f, 1.0f, 1.0f);
        spriteActors.push_back(&sprite);
    }

    // row_00
    spriteActors[0]->animFrames = {
        MakeAtlasRect(  2.0f,  8.0f, 18.0f, 23.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect( 24.0f,  7.0f, 19.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect( 47.0f,  7.0f, 19.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect( 70.0f,  8.0f, 18.0f, 23.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect( 92.0f,  8.0f, 19.0f, 23.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect(114.0f,  7.0f, 19.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect(138.0f,  7.0f, 17.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect(160.0f, 11.0f, 19.0f, 20.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect(209.0f,  7.0f, 18.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
    };
    // row_02
    spriteActors[1]->animFrames = {
        MakeAtlasRect(  3.0f, 36.0f, 18.0f, 23.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect( 25.0f, 35.0f, 18.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect( 47.0f, 35.0f, 19.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect( 70.0f, 36.0f, 18.0f, 23.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect( 93.0f, 36.0f, 18.0f, 23.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect(114.0f, 35.0f, 19.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect(139.0f, 36.0f, 17.0f, 23.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect(160.0f, 40.0f, 19.0f, 19.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect(210.0f, 35.0f, 17.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
    };
    // row_04
    spriteActors[2]->animFrames = {
        MakeAtlasRect(  2.0f, 64.0f, 18.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect( 25.0f, 64.0f, 18.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect( 47.0f, 64.0f, 19.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect( 70.0f, 64.0f, 18.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect( 92.0f, 64.0f, 19.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect(114.0f, 64.0f, 19.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect(139.0f, 64.0f, 14.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect(159.0f, 69.0f, 20.0f, 19.0f, SpriteSheetWidth, SpriteSheetHeight),
        MakeAtlasRect(210.0f, 64.0f, 17.0f, 24.0f, SpriteSheetWidth, SpriteSheetHeight),
    };

    // -------------------- GAME LOOP -------------------
    bool running = true;
    const auto startTime = std::chrono::steady_clock::now();
    auto prevTime = startTime;
    while (running)
    {
        if (!window.PumpMessages())
        {
            running = false;
            break;
        }

        if (window.IsMinimized())
        {
            Sleep(10);
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        const float t  = std::chrono::duration<float>(now - startTime).count();
        const float dt = std::chrono::duration<float>(now - prevTime).count();
        prevTime = now;

        for (size_t i = 0; i < cubeActors.size(); ++i)
        {
            Entity* entity = cubeActors[i];
            if (!entity)
                continue;

            const float phase = static_cast<float>(i) * 0.9f;

            const float x = -2.0f + static_cast<float>(i) * 2.0f;
            const float y = sinf(t * 1.6f + phase) * 0.35f;
            const float z = -5.0f;
            entity->transform.SetPosition(x, y, z);

            const float angle = t * (0.7f + static_cast<float>(i) * 0.35f) + phase;
            entity->transform.SetRotation(QuatRotationAxis(vec3(0.0f, 1.0f, 0.0f), angle));
        }

        for (size_t i = 0; i < spriteActors.size(); ++i)
        {
            Entity* sprite = spriteActors[i];
            if (!sprite)
                continue;

            const float phase = static_cast<float>(i) * 0.55f + 1.1f;
            const float x = -2.8f + static_cast<float>(i) * 2.8f;
            const float y = 0.45f + sinf(t * 1.45f + phase) * 0.18f;
            const float z = -3.4f + cosf(t * 0.70f + phase) * 0.20f;
            sprite->transform.SetPosition(x, y, z);
        }

        for (Entity* sprite : spriteActors)
        {
            if (!sprite || sprite->animFrames.empty())
                continue;

            sprite->animTimer += dt;
            const int frameIndex = static_cast<int>(sprite->animTimer * sprite->animSpeed)
                                   % static_cast<int>(sprite->animFrames.size());
            sprite->spriteUVRect = sprite->animFrames[frameIndex];
        }

        const float camAngle = t * 0.25f;
        const float camRadius = 8.0f;
        const float camHeight = 2.0f + sinf(t * 0.45f) * 0.35f;
        const vec3 camTarget(0.0f, 0.0f, -5.0f);
        const vec3 camPos(
            camTarget.x + cosf(camAngle) * camRadius,
            camHeight,
            camTarget.z + sinf(camAngle) * camRadius);
        dx12.SetCamera(camPos, camTarget);

        dx12.BeginFrame();
        dx12.RenderScene(&scene);
        dx12.EndFrame();
    }

    dx12.Shutdown();
    window.Shutdown();
    if (shouldUninitializeCom)
        CoUninitialize();
    return 0;
}