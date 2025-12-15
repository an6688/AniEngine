#pragma once

#include <windows.h>
#include <string>

// Forward declaration
class Input;

// Win32 window wrapper
// DEPENDS ON: Nothing, just Win32 API
class Window
{
public:
    Window();
    ~Window();

    // Initialize the window
    bool Initialize(const wchar_t* title, int width, int height);

    // Shutdown and destroy window
    void Shutdown();

    // Process Windows messages (call once per frame)
    // Returns false if window should close
    bool ProcessMessages();

    // Get window handle for DirectX initialization
    void SetInputHandler(Input* input) { m_input = input; }

    // Get window handle
    HWND GetHandle() const { return m_hwnd; }

    // Get window dimensions
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    // Check if window is active/focused
    bool IsActive() const { return m_isActive; }

    // Update window title (useful for FPS display)
    void SetTitle(const wchar_t* title);

private:
    // Win32 window callback
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Instance-specific message handler
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hwnd;
    int m_width;
    int m_height;
    bool m_isActive;
    std::wstring m_title;
    Input* m_input;
};