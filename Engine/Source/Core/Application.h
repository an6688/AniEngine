#pragma once

#include "Window.h"
#include "Timer.h"
#include "Input.h"
#include <vector>
#include <memory>
#include "Rendering/TextureManager.h"
#include "Rendering/Material.h"
#include "Rendering/ModelLoader.h"

class RenderDevice;
class Renderer;
class Mesh;
class Camera;

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

    // Handle camera input
    void UpdateCameraInput();

private:
    Window m_window;
    Timer m_timer;
    Input m_input;
    RenderDevice* m_renderDevice;  // Heap allocated to avoid header include
    Renderer* m_renderer;          // High-level rendering
    Camera* m_camera;              // Camera system

    // Texture and model management
    TextureManager* m_textureManager;
    LoadedModel m_loadedModel;

    // Keep for backwards compatibility with LoadTestModel
    std::vector<std::unique_ptr<Mesh>> m_loadedMeshes;
    float m_modelRotation;  // For spinning the model

    bool m_isRunning;
    float m_titleUpdateTimer;  // Update title every 0.5 seconds
};