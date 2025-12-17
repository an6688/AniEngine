#include "Application.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/Renderer.h"
#include "Rendering/Mesh.h"
#include "Rendering/ModelLoader.h"
#include "Rendering/Camera.h"
#include "Rendering/Texture.h"
#include <sstream>
#include <iomanip>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>


Application::Application()
    : m_renderDevice(nullptr)
    , m_renderer(nullptr)
    , m_camera(nullptr)
    , m_textureManager(nullptr)
    , m_isRunning(false)
    , m_titleUpdateTimer(0.0f)
    , m_modelRotation(0.0f)
{
}

Application::~Application()
{
    Shutdown();

    if (m_camera)
    {
        delete m_camera;
        m_camera = nullptr;
    }

    if (m_renderer)
    {
        delete m_renderer;
        m_renderer = nullptr;
    }

    if (m_textureManager)
    {
        m_textureManager->Shutdown();
        delete m_textureManager;
        m_textureManager = nullptr;
    }

    if (m_renderDevice)
    {
        delete m_renderDevice;
        m_renderDevice = nullptr;
    }
}

bool Application::Initialize()
{
    // Initialize window
    if (!m_window.Initialize(L"AniEngine", 1280, 720))
    {
        return false;
    }

    // Connect input to window
    m_input.Initialize();
    m_window.SetInputHandler(&m_input);

    // Create and initialize render device
    m_renderDevice = new RenderDevice();
    if (!m_renderDevice->Initialize(m_window.GetHandle(), m_window.GetWidth(), m_window.GetHeight()))
    {
        return false;
    }

    // Create and initialize renderer
    m_renderer = new Renderer();
    if (!m_renderer->Initialize(m_renderDevice))
    {
        return false;
    }

    // Create texture manager
    m_textureManager = new TextureManager();
    if (!m_textureManager->Initialize(m_renderDevice))
    {
        return false;
    }

    // Create camera
    m_camera = new Camera();
    m_camera->Initialize(1280.0f / 720.0f);
    m_camera->FrameBounds(glm::vec3(0.0f, 0.0f, 0.0f), 2.0f);  // Default framing

    // Load test model (avocado)
    const char* modelPath = "E:/repos/glTF-Sample-Assets-main/glTF-Sample-Assets-main/Models/Avocado/glTF/Avocado.gltf";

    ModelLoader loader;
    loader.SetTextureManager(m_textureManager);

    if (!loader.LoadGLTF(modelPath, m_renderDevice, m_loadedModel))
    {
        MessageBoxA(nullptr, "Failed to load avocado model! Check the path.", "Warning", MB_OK | MB_ICONWARNING);
    }
    else
    {
        // Debug: print how many textures loaded
        char msg[256];
        sprintf_s(msg, "Loaded %zu meshes, %zu materials, %zu textures\n",
            m_loadedModel.meshes.size(),
            m_loadedModel.materials.size(),
            m_loadedModel.textures.size());
        OutputDebugStringA(msg);
    }

    // Start timer
    m_timer.Start();
    m_timer.Tick();  // First tick

    m_isRunning = true;

    return true;
}

void Application::Run()
{
    // Main game loop
    while (m_isRunning)
    {
        if (!m_window.ProcessMessages())
        {
            m_isRunning = false;
            break;
        }

        m_input.Update();

        m_timer.Tick();

        // Update game logic
        Update();

        // Render frame
        Render();
    }
}

void Application::Shutdown()
{
    if (m_renderer)
    {
        m_renderer->Shutdown();
    }

    if (m_renderDevice)
    {
        m_renderDevice->Shutdown();
    }

    m_window.Shutdown();
}

void Application::Update()
{
    float deltaTime = m_timer.GetDeltaTime();

    // Handle camera input
    UpdateCameraInput();

    // Update camera
    m_camera->Update(deltaTime);

    // Uncomment to rotate model:
    // m_modelRotation += deltaTime * 0.2f;

    UpdateWindowTitle();
}

void Application::UpdateCameraInput()
{
    // Left mouse button - orbit
    if (m_input.IsMouseButtonDown(0))  // Left mouse
    {
        float deltaX = static_cast<float>(m_input.GetMouseDeltaX());
        float deltaY = static_cast<float>(m_input.GetMouseDeltaY());

        m_camera->Orbit(deltaX * 0.01f, -deltaY * 0.01f);
    }

    // Middle mouse or Shift+Left - pan
    bool panMode = m_input.IsMouseButtonDown(2) ||  // Middle mouse
        (m_input.IsMouseButtonDown(0) && m_input.IsKeyDown(VK_SHIFT));  // Shift+Left

    if (panMode)
    {
        float deltaX = static_cast<float>(-m_input.GetMouseDeltaX());
        float deltaY = static_cast<float>(m_input.GetMouseDeltaY());
        m_camera->Pan(deltaX, deltaY);
    }

    // Mouse wheel - zoom
    int wheelDelta = m_input.GetMouseWheelDelta();
    if (wheelDelta != 0)
    {
        m_camera->Zoom(static_cast<float>(-wheelDelta) * 0.5f);
    }

    // F key - frame model
    if (m_input.IsKeyDown('F'))
    {
        m_camera->FrameBounds(glm::vec3(0.0f, 0.0f, 0.0f), 2.0f);
    }

    // WASD keyboard movement
    {
        float deltaTime = m_timer.GetDeltaTime();
        float moveAmount = 30.0f * deltaTime;
        if (m_input.IsKeyDown('A')) m_camera->Pan(-moveAmount, 0.0f);
        if (m_input.IsKeyDown('D')) m_camera->Pan(moveAmount, 0.0f);
        if (m_input.IsKeyDown('Q')) m_camera->Pan(0.0f, -moveAmount);
        if (m_input.IsKeyDown('E')) m_camera->Pan(0.0f, moveAmount);
        if (m_input.IsKeyDown('W')) m_camera->PanForward(moveAmount);
        if (m_input.IsKeyDown('S')) m_camera->PanForward(-moveAmount);
    }
}

void Application::Render()
{
    m_renderDevice->BeginFrame();

    // Create transform for the model
    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::scale(transform, glm::vec3(30.0f));
    transform = glm::rotate(transform, m_modelRotation, glm::vec3(0.0f, 1.0f, 0.0f));

    // Render loaded model
    if (!m_loadedModel.meshes.empty())
    {
        for (const auto& mesh : m_loadedModel.meshes)
        {
            m_renderer->DrawMesh(mesh.get(), transform, m_camera);
        }
    }
    else
    {
        // Fallback: draw cube if no model loaded
        m_renderer->DrawCube(m_timer.GetDeltaTime());
    }

    m_renderDevice->EndFrame();
}

void Application::UpdateWindowTitle()
{
    m_titleUpdateTimer += m_timer.GetDeltaTime();

    if (m_titleUpdateTimer >= 0.5f)
    {
        std::wostringstream title;
        title << L"AniEngine | FPS: " << std::fixed << std::setprecision(1)
            << m_timer.GetFPS()
            << L" | Meshes: " << m_loadedModel.meshes.size()
            << L" | Textures: " << m_loadedModel.textures.size();

        m_window.SetTitle(title.str().c_str());
        m_titleUpdateTimer = 0.0f;
    }
}

bool Application::LoadTestModel(const char* filepath)
{
    ModelLoader loader;

    if (!loader.LoadGLTF(filepath, m_renderDevice, m_loadedMeshes))
    {
        return false;
    }

    return !m_loadedMeshes.empty();
}