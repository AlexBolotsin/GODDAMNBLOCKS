#include "GrmWindowWrapper.h"
#include <stdexcept>
#include <cassert>

bool GRMWindowWrapper::Init(HINSTANCE hInstance, const WindowDesc& desc)
{
    m_hInstance = hInstance;
    m_width = desc.width;
    m_height = desc.height;
    m_isFullScreen = desc.isFullScreen;

    QueryNativeResolution();
    
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW; // repaint on resize
    wc.lpfnWndProc = WndProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = CLASS_NAME;
    wc.lpszMenuName = nullptr;

    if (!RegisterClassExW(&wc))
    {
        MessageBoxW(nullptr, L"Failed to register window class", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    DWORD style = desc.isFullScreen ? (WS_POPUP | WS_VISIBLE) : (WS_OVERLAPPEDWINDOW);

    RECT rc = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };

    if (!desc.isFullScreen)
    {
        AdjustWindowRect(&rc, style, false);
    }

    m_hWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        desc.title.c_str(),
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr, nullptr, m_hInstance, this
    );

    if (!m_hWnd)
    {
        DWORD err = GetLastError();
        wchar_t buf[128];
        swprintf_s(buf, L"CreateWindowExW failed. Error: %lu\n", err);
        OutputDebugStringW(buf);
        MessageBoxW(nullptr, buf, L"Init Error", MB_OK | MB_ICONERROR);
        return false;
    }

    RegisterRawMouseInput();

    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);
    SetForegroundWindow(m_hWnd);
    SetFocus(m_hWnd);

    if (!desc.showCursor)
    {
        ShowCursor(false);
        ConfineCursor(true);
    }

    if (desc.isFullScreen)
    {
        ApplyFullscreen(true);
    }

    return true;
}

void GRMWindowWrapper::Shutdown()
{
    ShowCursor(true);
    ConfineCursor(false);

    if (m_hWnd)
    {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }

    UnregisterClassW(CLASS_NAME, m_hInstance);
}

bool GRMWindowWrapper::PumpMessages()
{
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            return false;

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
}

LRESULT CALLBACK GRMWindowWrapper::WndProc(HWND hwnd, UINT msg,
                                            WPARAM wp, LPARAM lp)
{
    GRMWindowWrapper* self = nullptr;

    if (msg == WM_NCCREATE)
    {
        // Guard against null lp
        if (!lp) return DefWindowProcW(hwnd, msg, wp, lp);

        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);

        // Guard against null lpCreateParams
        if (!cs->lpCreateParams) return DefWindowProcW(hwnd, msg, wp, lp);

        self = reinterpret_cast<GRMWindowWrapper*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(self));

        // CRITICAL — must call DefWindowProc for WM_NCCREATE
        // so Windows finishes setting up the non-client area
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    else
    {
        self = reinterpret_cast<GRMWindowWrapper*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self)
        return self->HandleMessage(hwnd, msg, wp, lp);

    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT GRMWindowWrapper::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;

        case WM_DESTROY:
            if (OnDestroy)
                OnDestroy();
            return 0;

        case WM_ENTERSIZEMOVE:
            m_isResizing = true;
            return 0;
            
        case WM_EXITSIZEMOVE:
            m_isResizing = false;
            if (OnResize)
                OnResize(m_width, m_height);
            return 0;
        
        case WM_SIZE:
        {
            m_width = LOWORD(lParam);
            m_height = HIWORD(lParam);
            m_isMinimized = (wParam == SIZE_MINIMIZED);

            if (!m_isResizing && !m_isMinimized && (m_width > 0) && (m_height > 0))
            {
                if (OnResize)
                {
                     OnResize(m_width, m_height);
                }
            }
            return 0;
        }

        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* pMinMax = reinterpret_cast<MINMAXINFO*>(lParam);
            pMinMax->ptMinTrackSize.x = 320;
            pMinMax->ptMinTrackSize.y = 240;
            return 0;
        }

        case WM_INPUT:
        {
            // Handle raw mouse input here if needed
            break;
        }

        case WM_SYSKEYDOWN:
        {
            if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000) // Alt + Enter
            {
                ApplyFullscreen(!m_isFullScreen);
                return 0;
            }
            break;
        }

        case WM_SYSCOMMAND:
        {
            if ((wParam & 0xFFF0) == SC_SCREENSAVE || (wParam & 0xFFF0) == SC_MONITORPOWER) // Disable ALT application menu
                return 0;
            break;
        }

        case WM_SETFOCUS:
        {
            ConfineCursor(true);
            break;
        }

        case WM_KILLFOCUS:
        {
            ConfineCursor(false);
            break;
        }


        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            // Fill with a dark grey so you know WM_PAINT is firing
            HBRUSH brush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hdc, &ps.rcPaint, brush);
            DeleteObject(brush);

            // Draw some text to confirm the window is alive
            SetTextColor(hdc, RGB(0, 255, 0));
            SetBkMode(hdc, TRANSPARENT);
            TextOutW(hdc, 20, 20, L"Window OK - DX12 not yet initialized", 36);

            EndPaint(hWnd, &ps);
            return 0;
        }
    }
    return DefWindowProcW(m_hWnd, message, wParam, lParam);
}

void GRMWindowWrapper::QueryNativeResolution()
{
    DEVMODEW dm = {};
    dm.dmSize = sizeof(DEVMODEW);
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm))
    {
        m_nativeWidth = dm.dmPelsWidth;
        m_nativeHeight = dm.dmPelsHeight;
        m_refreshRate = dm.dmDisplayFrequency;
    }
    else
    {
        m_nativeWidth = static_cast<uint32_t>(GetSystemMetrics(SM_CXSCREEN));
        m_nativeHeight = static_cast<uint32_t>(GetSystemMetrics(SM_CYSCREEN));
        m_refreshRate = 60;
    }
}

void GRMWindowWrapper::RegisterRawMouseInput()
{
    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = 0x01; // Generic desktop controls
    rid.usUsage = 0x02;     // Mouse
    rid.dwFlags = RIDEV_INPUTSINK; // Receive input even when not in the foreground
    rid.hwndTarget = m_hWnd;

    if (RegisterRawInputDevices(&rid, 1, sizeof(rid)) != 0)
    {
        OutputDebugStringW(L"Warning: failed to register raw mouse input.\n");
    }
}

void GRMWindowWrapper::ApplyFullscreen(bool enable)
{
    m_isFullScreen = enable;

    if (enable)
    {
        GetWindowRect(m_hWnd, &m_windowedRect); // Save current window position and size
        
        // Set fullscreen style
        SetWindowLongW(m_hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(m_hWnd,
            HWND_TOP, 0, 0,
            static_cast<int>(m_nativeWidth),
            static_cast<int>(m_nativeHeight),
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        m_width = m_nativeWidth;
        m_height = m_nativeHeight;
    }
    else
    {
        // Restore windowed style and position
        SetWindowLongW(m_hWnd, GWL_STYLE, WS_VISIBLE);

        SetWindowPos(m_hWnd, nullptr, m_windowedRect.left, m_windowedRect.top,
                     m_windowedRect.right - m_windowedRect.left,
                     m_windowedRect.bottom - m_windowedRect.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                     
        RedrawWindow(m_hWnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);

        ShowWindow(m_hWnd, SW_RESTORE);
        SetForegroundWindow(m_hWnd);
        
        m_width = m_windowedRect.right - m_windowedRect.left;
        m_height = m_windowedRect.bottom - m_windowedRect.top;
    }
    
    if (OnResize) OnResize(m_width, m_height);
}

void GRMWindowWrapper::ShowCursor(bool show) const
{
    while (show ? (::ShowCursor(true) < 0) : (::ShowCursor(false) >= 0));
}

void GRMWindowWrapper::ConfineCursor(bool confine) const
{
    if (confine && m_hWnd)
    {
        RECT rc;
        GetClientRect(m_hWnd, &rc);
        MapWindowPoints(m_hWnd, nullptr, reinterpret_cast<POINT*>(&rc), 2);
        ClipCursor(&rc);
    }
    else
    {
        ClipCursor(nullptr);
    }
}