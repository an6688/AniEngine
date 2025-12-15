#pragma once

#include "Window.h"
#include "Timer.h"
#include <vector>
#include <memory>

// Forward declarations to avoid including full headers
class RenderDevice;
class Renderer;
class Mesh;

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

    // Load a model for testing
    bool LoadTestModel(const char* filepath);

private:
    Window m_window;
    Timer m_timer;
    RenderDevice* m_renderDevice;  // Heap allocated to avoid header include
    Renderer* m_renderer;          // High-level rendering

    // Loaded meshes (owned by Application, not Renderer)
    std::vector<std::unique_ptr<Mesh>> m_loadedMeshes;
    float m_modelRotation;  // For spinning the model

    bool m_isRunning;
    float m_titleUpdateTimer;  // Update title every 0.5 seconds
};