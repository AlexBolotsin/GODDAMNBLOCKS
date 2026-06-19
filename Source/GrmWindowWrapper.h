#pragma once

#include <windows.h>
#include <cstdio>
#include <string>
#include <functional>

struct WindowDesc
{
    std::wstring title = L"MazeGame";
    uint32_t width = 1280;
    uint32_t height = 720;
    bool isFullScreen = false;
    bool showCursor = false;
};

class GRMWindowWrapper
{
public:
    std::function<void(uint32_t, uint32_t)> OnResize;
    std::function<void()> OnDestroy;

    bool Init(HINSTANCE hInstance, const WindowDesc& desc);
    void Shutdown();

    bool PumpMessages();

    HWND GetHWND() const { return m_hWnd; }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    float GetAspectRatio() const { return static_cast<float>(m_width) / m_height; }

    bool IsMinimized() const { return m_isMinimized; }

    uint32_t GetNativeWidth() const { return m_nativeWidth; }
    uint32_t GetNativeHeight() const { return m_nativeHeight; }
    uint32_t GetRefreshRate() const { return m_refreshRate; }

    void ShowCursor(bool show) const;
    void ConfineCursor(bool confine) const;

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    void QueryNativeResolution();
    void RegisterRawMouseInput();
    void ApplyFullscreen(bool enable);

    HWND m_hWnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    RECT m_windowedRect = { 0, 0, 1280, 720 };

    uint32_t m_width = 1280;
    uint32_t m_height = 720;

    uint32_t m_nativeWidth = 0;
    uint32_t m_nativeHeight = 0;
    uint32_t m_refreshRate = 60;

    bool m_isMinimized = false;
    bool m_isResizing = false;
    bool m_isFullScreen = false;

    static constexpr wchar_t CLASS_NAME[] = L"MazeGameWindow";

};