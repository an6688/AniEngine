#pragma once

#include "Window.h"
#include "Timer.h"
#include "Input.h"
#include <vector>
#include <memory>
#include "Rendering/TextureManager.h"
#include "Rendering/Material.h"
#include "Rendering/ModelLoader.h"
#include "Rendering/Renderer.h"
#include "ImGuiManager.h"
#include "Scene/SceneManager.h"


class RenderDevice;
class Renderer;
class Camera;
class ImGuiManager;

class Application
{
public:
    Application();
    ~Application();

    bool Initialize();
    void Run();
    void Shutdown();

private:
    void Update();
    void Render();
    void UpdateCameraInput();
    void UpdateWindowTitle();
    void FrameScene();
    void DrawDebugUI();
    void LoadModel(const std::string& path);

private:
    Window m_window;
    Input m_input;
    Timer m_timer;

    RenderDevice* m_renderDevice;
    Renderer* m_renderer;
    Camera* m_camera;
    TextureManager* m_textureManager;
    ImGuiManager* m_imguiManager;
    SceneManager m_sceneManager;
    RenderSettings m_renderSettings;

    // Loaded model data
    LoadedModel m_loadedModel;

    // Model transform parameters
    glm::vec3 m_modelCenter;
    float m_modelScale;
    float m_modelRotation;

    bool m_isRunning;
    bool m_fKeyWasDown = false;
    bool m_resizePending = false;
    int m_pendingWidth = 0;
    int m_pendingHeight = 0;

    float m_titleUpdateTimer;
};