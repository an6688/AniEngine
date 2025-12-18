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

Application::Application()
    : m_renderDevice(nullptr)
    , m_renderer(nullptr)
    , m_camera(nullptr)
    , m_textureManager(nullptr)
    , m_isRunning(false)
    , m_titleUpdateTimer(0.0f)
    , m_modelRotation(0.0f)
    , m_modelScale(1.0f)
    , m_modelCenter(0.0f)
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

    // Create texture manager BEFORE renderer
    m_textureManager = new TextureManager();
    if (!m_textureManager->Initialize(m_renderDevice))
    {
        return false;
    }

    // Create and initialize renderer
    m_renderer = new Renderer();
    if (!m_renderer->Initialize(m_renderDevice, m_textureManager))
    {
        return false;
    }

    // Create camera
    m_camera = new Camera();
    m_camera->Initialize(1280.0f / 720.0f);

    // Load test model (avocado)
    const char* modelPath = "E:/repos/glTF-Sample-Assets-main/glTF-Sample-Assets-main/Models/Avocado/glTF/Avocado.gltf";

    ModelLoader loader;
    loader.SetTextureManager(m_textureManager);

    if (!loader.LoadGLTF(modelPath, m_renderDevice, m_loadedModel))
    {
        MessageBoxA(nullptr, "Failed to load model! Check the path.", "Warning", MB_OK | MB_ICONWARNING);
        // Still allow running with fallback cube
    }
    else
    {
        char msg[256];
        sprintf_s(msg, "Loaded %zu meshes, %zu materials, %zu textures\n",
            m_loadedModel.meshes.size(),
            m_loadedModel.materials.size(),
            m_loadedModel.textures.size());
        OutputDebugStringA(msg);

        // Frame the loaded model properly
        FrameModel();
    }

    // Start timer
    m_timer.Start();
    m_timer.Tick();

    m_isRunning = true;

    return true;
}

void Application::FrameModel()
{
    if (m_loadedModel.meshes.empty())
    {
        m_camera->FrameBounds(glm::vec3(0.0f), 2.0f);
        m_modelScale = 1.0f;
        m_modelCenter = glm::vec3(0.0f);
        return;
    }

    // Store the original model center for transform
    m_modelCenter = m_loadedModel.center;

    // Calculate the radius (half the diagonal)
    float modelRadius = m_loadedModel.size * 0.5f;

    // We'll normalize the model scale so it fits in a reasonable size
    // But the camera will frame based on the ACTUAL model bounds
    if (m_loadedModel.size > 0.001f)
    {
        // Normalize to roughly 2 units
        m_modelScale = 2.0f / m_loadedModel.size;
    }
    else
    {
        m_modelScale = 1.0f;
    }

    // After scaling, the model will be centered at origin with radius ~1
    // Frame the camera on this normalized model
    float scaledRadius = modelRadius * m_modelScale;
    m_camera->FrameBounds(glm::vec3(0.0f), scaledRadius);

    char msg[256];
    sprintf_s(msg, "FrameModel: original center(%.2f, %.2f, %.2f), size: %.2f, scale: %.3f, scaled radius: %.2f\n",
        m_modelCenter.x, m_modelCenter.y, m_modelCenter.z,
        m_loadedModel.size, m_modelScale, scaledRadius);
    OutputDebugStringA(msg);
}

void Application::Run()
{
    while (m_isRunning)
    {
        if (!m_window.ProcessMessages())
        {
            m_isRunning = false;
            break;
        }

        m_input.Update();
        m_timer.Tick();

        Update();
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

    UpdateCameraInput();
    m_camera->Update(deltaTime);

    // Uncomment to rotate model:
    // m_modelRotation += deltaTime * 0.5f;

    UpdateWindowTitle();
}

void Application::UpdateCameraInput()
{
    float deltaTime = m_timer.GetDeltaTime();

    // Speed multiplier - Shift for fast, Ctrl for slow
    float speedMultiplier = 1.0f;
    if (m_input.IsKeyDown(VK_SHIFT))
    {
        speedMultiplier = 5.0f;
    }
    if (m_input.IsKeyDown(VK_CONTROL))
    {
        speedMultiplier = 0.2f;
    }

    // Left mouse button - orbit (when not holding shift)
    if (m_input.IsMouseButtonDown(0) && !m_input.IsKeyDown(VK_SHIFT))
    {
        float deltaX = static_cast<float>(m_input.GetMouseDeltaX());
        float deltaY = static_cast<float>(m_input.GetMouseDeltaY());
        m_camera->Orbit(deltaX * 0.01f, -deltaY * 0.01f);
    }

    // Middle mouse or Shift+Left - pan
    bool panMode = m_input.IsMouseButtonDown(2) ||
        (m_input.IsMouseButtonDown(0) && m_input.IsKeyDown(VK_SHIFT));

    if (panMode)
    {
        float deltaX = static_cast<float>(-m_input.GetMouseDeltaX());
        float deltaY = static_cast<float>(m_input.GetMouseDeltaY());
        m_camera->Pan(deltaX * speedMultiplier, deltaY * speedMultiplier);
    }

    // Mouse wheel - zoom
    int wheelDelta = m_input.GetMouseWheelDelta();
    if (wheelDelta != 0)
    {
        m_camera->Zoom(static_cast<float>(-wheelDelta) * 0.5f * speedMultiplier);
    }

    // F key - frame model (use IsKeyDown if IsKeyPressed not available)
    static bool fKeyWasDown = false;
    bool fKeyIsDown = m_input.IsKeyDown('F');
    if (fKeyIsDown && !fKeyWasDown)
    {
        FrameModel();
    }
    fKeyWasDown = fKeyIsDown;

    // WASD keyboard movement with speed multiplier
    {
        float moveAmount = 30.0f * deltaTime * speedMultiplier;
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

    // Create transform for the model:
    // 1. Translate to center at origin
    // 2. Scale uniformly
    // 3. Apply rotation
    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::scale(transform, glm::vec3(m_modelScale));
    transform = glm::rotate(transform, m_modelRotation, glm::vec3(0.0f, 1.0f, 0.0f));
    transform = glm::translate(transform, -m_modelCenter);

    // Render loaded model with PBR materials
    if (!m_loadedModel.meshes.empty())
    {
        for (const auto& mesh : m_loadedModel.meshes)
        {
            m_renderer->DrawMeshTextured(mesh.get(), transform, m_camera);
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
            << L" | Textures: " << m_loadedModel.textures.size()
            << L" | Dist: " << std::setprecision(2) << m_camera->GetDistance();

        m_window.SetTitle(title.str().c_str());
        m_titleUpdateTimer = 0.0f;
    }
}
