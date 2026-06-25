#include "GrmWindowWrapper.h"
#include "DX12Context.h"
#include "Game.h"
#include <chrono>
#include <filesystem>
#include <string>

namespace
{
    std::wstring GetShaderPath()
    {
        wchar_t modulePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        return (std::filesystem::path(modulePath).parent_path() / "Shaders" / "Material.hlsl").wstring();
    }

    std::wstring GetSpriteSheetPath()
    {
        wchar_t modulePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        std::filesystem::path path(modulePath);
        path = path.parent_path().parent_path().parent_path() / "Sprites" / "19338.png";
        return path.wstring();
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int /*nCmdShow*/)
{
    const HRESULT comInitHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitializeCom = SUCCEEDED(comInitHr);
    if (FAILED(comInitHr) && comInitHr != RPC_E_CHANGED_MODE)
    {
        OutputDebugStringW(L"Failed to initialize COM.\n");
        return -100;
    }

    WindowDesc desc;
    desc.title = L"MazeGame";
    desc.width = 1280;
    desc.height = 720;
    desc.isFullScreen = false;
    desc.showCursor = false;

    GRMWindowWrapper window;
    window.OnDestroy = [] { OutputDebugStringW(L"Window destroyed\n"); };

    if (!window.Init(hInstance, desc))
        return -1;

    DX12Context dx12;
    if (!dx12.Init(window.GetHWND(), window.GetWidth(), window.GetHeight()))
    {
        OutputDebugStringW(L"Failed to initialize DirectX 12.\n");
        return -1;
    }

    window.OnResize = [&dx12](uint32_t w, uint32_t h) { dx12.Resize(w, h); };

    Game game;
    if (!game.Init(dx12, GetShaderPath().c_str(), GetSpriteSheetPath().c_str()))
    {
        OutputDebugStringW(L"Failed to initialize game.\n");
        dx12.Shutdown();
        return -2;
    }

    auto prevTime = std::chrono::steady_clock::now();
    while (true)
    {
        if (!window.PumpMessages())
            break;

        if (window.IsMinimized())
        {
            Sleep(10);
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - prevTime).count();
        prevTime = now;

        game.Update(dt);

        dx12.BeginFrame();
        dx12.RenderScene(&game.GetScene(), game.GetCamera());
        dx12.EndFrame();
    }

    dx12.Shutdown();
    window.Shutdown();
    if (shouldUninitializeCom)
        CoUninitialize();
    return 0;
}
