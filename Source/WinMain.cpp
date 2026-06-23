#include "GrmWindowWrapper.h"
#include "DX12Context.h"
#include "Scene.h"
#include "Mesh.h"
#include "Material.h"
#include <stdio.h>
#include <memory>
#include <chrono>
#include <cmath>

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
        0, 1, 2, 0, 2, 3,       // Front
        4, 6, 5, 4, 7, 6,       // Back
        8, 9, 10, 8, 10, 11,    // Top
        12, 15, 14, 12, 14, 13, // Bottom
        16, 17, 18, 16, 18, 19, // Right
        20, 23, 22, 20, 22, 21  // Left
    };

    auto mesh = std::make_shared<Mesh>();
    if (!mesh->Init(device, vertices, indices))
        return nullptr;

    return mesh;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int /*nCmdShow*/)
{
    // Setup window
    WindowDesc desc;
    desc.title = L"MazeGame";
    desc.width = 1280;
    desc.height = 720;
    desc.isFullScreen = false;
    desc.showCursor = false;

    GRMWindowWrapper window;

    window.OnResize = [](uint32_t width, uint32_t height)
    {
        wchar_t buf[64];
        swprintf_s(buf, L"Window resized: %u x %u\n", width, height);
        OutputDebugStringW(buf);
    };

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
        MessageBoxW(nullptr, L"Failed to initialize DirectX 12.", L"DX12 Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    // Create scene and populate with test objects
    Scene scene;

    // Create shared mesh
    auto cubeMesh = CreateCubeMesh(dx12.GetDevice());

    if (!cubeMesh)
    {
        MessageBoxW(nullptr, L"Failed to create cube mesh.", L"Mesh Error", MB_OK | MB_ICONERROR);
        dx12.Shutdown();
        return -1;
    }

    // Create multiple cubes with different positions and colors
    for (int i = 0; i < 3; ++i)
    {
        auto material = std::make_shared<Material>();
        if (!material->Init(dx12.GetDevice()))
        {
            MessageBoxW(nullptr, L"Failed to initialize material.", L"Material Error", MB_OK | MB_ICONERROR);
            dx12.Shutdown();
            return -1;
        }

        Entity& entity = scene.CreateEntity();
        entity.mesh = cubeMesh;
        entity.material = material;
        entity.transform.SetPosition(-2.0f + i * 2.0f, 0.0f, -5.0f);
        entity.transform.SetScale(0.75f + i * 0.2f, 0.75f + i * 0.2f, 0.75f + i * 0.2f);
        entity.transform.SetRotation(QuatRotationAxis(vec3(0.0f, 1.0f, 0.0f), i * 0.35f));
        entity.material->color = vec4(
            0.2f + i * 0.3f,
            0.2f + (i % 2) * 0.5f,
            0.2f + ((i + 1) % 2) * 0.5f,
            1.0f
        );
    }

    // -------------------- GAME LOOP -------------------
    bool running = true;
    const auto startTime = std::chrono::steady_clock::now();
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
        const float t = std::chrono::duration<float>(now - startTime).count();

        for (size_t i = 0; i < scene.GetEntities().size(); ++i)
        {
            Entity* entity = scene.GetEntities()[i].get();
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
    return 0;
}