#pragma once

#include <windows.h>

// Input manager - tracks mouse and keyboard state
// DEPENDS ON: Win32 API only
class Input
{
public:
    Input();
    ~Input();

    // Initialize (call once)
    void Initialize();

    // Update input state (call once per frame, before processing messages)
    void Update();

    // Process a Windows message (call from Window message handler)
    void ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Mouse position (client coordinates)
    int GetMouseX() const { return m_mouseX; }
    int GetMouseY() const { return m_mouseY; }

    // Mouse delta (change since last frame)
    int GetMouseDeltaX() const { return m_mouseDeltaX; }
    int GetMouseDeltaY() const { return m_mouseDeltaY; }

    // Mouse buttons
    bool IsMouseButtonDown(int button) const;     // 0=left, 1=right, 2=middle
    bool IsMouseButtonPressed(int button) const;  // Pressed this frame
    bool IsMouseButtonReleased(int button) const; // Released this frame

    // Mouse wheel
    int GetMouseWheelDelta() const { return m_mouseWheelDelta; }

    // Keyboard
    bool IsKeyDown(int vkCode) const;
    bool IsKeyPressed(int vkCode) const;
    bool IsKeyReleased(int vkCode) const;

private:
    // Mouse state
    int m_mouseX;
    int m_mouseY;
    int m_mouseXPrev;
    int m_mouseYPrev;
    int m_mouseDeltaX;
    int m_mouseDeltaY;
    int m_mouseWheelDelta;

    bool m_mouseButtons[3];        // Current state
    bool m_mouseButtonsPrev[3];    // Previous frame state

    // Keyboard state (256 keys)
    bool m_keys[256];
    bool m_keysPrev[256];
};