#pragma once

#include "Window.h"
#include "Timer.h"

// Forward declaration to avoid including full header
class RenderDevice;
class Renderer;

// Main application class - coordinates engine subsystems
// DEPENDS ON: Window, Timer, RenderDevice, Renderer (but does not expose them in interface)
class Application
{
public:
    Application();
    ~Application();

    // Initialize the application
    bool Initialize();

    // Main run loop
    void Run();

    // Shutdown and cleanup
    void Shutdown();

private:
    // Called once per frame
    void Update();
    void Render();

    // Update FPS counter in window title
    void UpdateWindowTitle();

private:
    Window m_window;
    Timer m_timer;
    RenderDevice* m_renderDevice;   // Heap allocated to avoid header include
    Renderer* m_renderer;           // High-level rendering

    bool m_isRunning;
    float m_titleUpdateTimer;       // Update title every 0.5 seconds
};