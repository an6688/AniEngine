#include "Input.h"
#include <cstring>

Input::Input()
    : m_mouseX(0)
    , m_mouseY(0)
    , m_mouseXPrev(0)
    , m_mouseYPrev(0)
    , m_mouseDeltaX(0)
    , m_mouseDeltaY(0)
    , m_mouseWheelDelta(0)
{
    memset(m_mouseButtons, 0, sizeof(m_mouseButtons));
    memset(m_mouseButtonsPrev, 0, sizeof(m_mouseButtonsPrev));
    memset(m_keys, 0, sizeof(m_keys));
    memset(m_keysPrev, 0, sizeof(m_keysPrev));
}

Input::~Input()
{
}

void Input::Initialize()
{
    // Nothing to do yet
}

void Input::Update()
{
    // Calculate mouse delta
    m_mouseDeltaX = m_mouseX - m_mouseXPrev;
    m_mouseDeltaY = m_mouseY - m_mouseYPrev;

    // Update previous frame state
    m_mouseXPrev = m_mouseX;
    m_mouseYPrev = m_mouseY;
    memcpy(m_mouseButtonsPrev, m_mouseButtons, sizeof(m_mouseButtons));
    memcpy(m_keysPrev, m_keys, sizeof(m_keys));

    // Reset wheel delta
    m_mouseWheelDelta = 0;
}

void Input::ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_MOUSEMOVE:
        m_mouseX = LOWORD(lParam);
        m_mouseY = HIWORD(lParam);
        break;

    case WM_LBUTTONDOWN:
        m_mouseButtons[0] = true;
        break;

    case WM_LBUTTONUP:
        m_mouseButtons[0] = false;
        break;

    case WM_RBUTTONDOWN:
        m_mouseButtons[1] = true;
        break;

    case WM_RBUTTONUP:
        m_mouseButtons[1] = false;
        break;

    case WM_MBUTTONDOWN:
        m_mouseButtons[2] = true;
        break;

    case WM_MBUTTONUP:
        m_mouseButtons[2] = false;
        break;

    case WM_MOUSEWHEEL:
        m_mouseWheelDelta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        break;

    case WM_KEYDOWN:
        if (wParam < 256)
            m_keys[wParam] = true;
        break;

    case WM_KEYUP:
        if (wParam < 256)
            m_keys[wParam] = false;
        break;
    }
}

bool Input::IsMouseButtonDown(int button) const
{
    if (button < 0 || button >= 3)
        return false;
    return m_mouseButtons[button];
}

bool Input::IsMouseButtonPressed(int button) const
{
    if (button < 0 || button >= 3)
        return false;
    return m_mouseButtons[button] && !m_mouseButtonsPrev[button];
}

bool Input::IsMouseButtonReleased(int button) const
{
    if (button < 0 || button >= 3)
        return false;
    return !m_mouseButtons[button] && m_mouseButtonsPrev[button];
}

bool Input::IsKeyDown(int vkCode) const
{
    if (vkCode < 0 || vkCode >= 256)
        return false;
    return m_keys[vkCode];
}

bool Input::IsKeyPressed(int vkCode) const
{
    if (vkCode < 0 || vkCode >= 256)
        return false;
    return m_keys[vkCode] && !m_keysPrev[vkCode];
}

bool Input::IsKeyReleased(int vkCode) const
{
    if (vkCode < 0 || vkCode >= 256)
        return false;
    return !m_keys[vkCode] && m_keysPrev[vkCode];
}