#include "GrmWindowWrapper.h"
#include "DX12Context.h"
#include <stdio.h>

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

    // -------------------- GAME LOOP -------------------
    bool running = true;
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

        dx12.Present();
    }

    dx12.Shutdown();
    window.Shutdown();
    return 0;
}