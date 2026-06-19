#include "GrmWindowWrapper.h"
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

    //DX12Context dx12;
    //dx12.Init(window.GetHWND(), window.GetWidth(), window.GetHeight());

    // -------------------- GAYME LOOP -------------------
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

        // TODO: Engine::Update(timer.DeltaTime())
        // TODO: Engine::Render()
        // TODO: dx12.Present()
    }

    // --- Shutdown -----------------------------------------------
    // dx12.shutdown();
    window.Shutdown();
    return 0;
}