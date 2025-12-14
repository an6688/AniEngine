#include "Window.h"

Window::Window()
    : m_hwnd(nullptr)
    , m_width(0)
    , m_height(0)
    , m_isActive(true)
{
}

Window::~Window()
{
    Shutdown();
}

bool Window::Initialize(const wchar_t* title, int width, int height)
{
    m_width = width;
    m_height = height;
    m_title = title;

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = L"GameEngineWindowClass";

    if (!RegisterClassExW(&wc))
    {
        return false;
    }

    // Calculate window size to get desired client area
    RECT rect = { 0, 0, width, height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;

    // Create window
    m_hwnd = CreateWindowExW(
        0,
        L"GameEngineWindowClass",
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth, windowHeight,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        this  // Pass 'this' pointer to WM_CREATE
    );

    if (!m_hwnd)
    {
        return false;
    }

    // Show the window
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    return true;
}

void Window::Shutdown()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

bool Window::ProcessMessages()
{
    MSG msg = {};

    // Process all messages in queue
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            return false;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return true;
}

void Window::SetTitle(const wchar_t* title)
{
    m_title = title;
    if (m_hwnd)
    {
        SetWindowTextW(m_hwnd, title);
    }
}

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Window* window = nullptr;

    if (msg == WM_CREATE)
    {
        // Get the Window* passed to CreateWindowEx
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<Window*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }
    else
    {
        // Retrieve the Window* from window data
        window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window)
    {
        return window->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT Window::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_ACTIVATE:
        m_isActive = (LOWORD(wParam) != WA_INACTIVE);
        return 0;

    case WM_SIZE:
        m_width = LOWORD(lParam);
        m_height = HIWORD(lParam);
        // TODO: Handle resize for DirectX swap chain
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        return 0;
    }

    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}