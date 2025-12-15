#include "Application.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/Renderer.h"
#include "Rendering/Mesh.h"
#include "Rendering/ModelLoader.h"
#include "Rendering/Camera.h"
#include <sstream>
#include <iomanip>
#include <glm/gtc/matrix_transform.hpp>

Application::Application()
    : m_renderDevice(nullptr)
    , m_renderer(nullptr)
    , m_camera(nullptr)
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

    // Create camera
    m_camera = new Camera();
    m_camera->Initialize(1280.0f / 720.0f);
    m_camera->FrameBounds(glm::vec3(0.0f, 0.0f, 0.0f), 2.0f);  // Default framing

    // Load test model (avocado)
    const char* modelPath = "E:/repos/glTF-Sample-Assets-main/glTF-Sample-Assets-main/Models/Avocado/glTF/Avocado.gltf";
    if (!LoadTestModel(modelPath))
    {
        MessageBoxA(nullptr, "Failed to load avocado model! Check the path.", "Warning", MB_OK | MB_ICONWARNING);
        // Continue anyway - will just show nothing
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

    // Rotate the model slowly, can disable with camera control
    // m_modelRotation += deltaTime * 0.2f;

    // Update window title with FPS
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
    if (m_input.IsKeyPressed('F'))
    {
        m_camera->FrameBounds(glm::vec3(0.0f, 0.0f, 0.0f), 2.0f);
    }

    // WASD keyboard movement (camera-space pan)
    {
        float deltaTime = m_timer.GetDeltaTime();
        float moveSpeed = 300.0f; // units per second

        float moveX = 0.0f;
        float moveY = 0.0f;

        if (m_input.IsKeyDown('W')) moveY += 1.0f;
        if (m_input.IsKeyDown('S')) moveY -= 1.0f;
        if (m_input.IsKeyDown('A')) moveX -= 1.0f;
        if (m_input.IsKeyDown('D')) moveX += 1.0f;

        if (moveX != 0.0f || moveY != 0.0f)
        {
            m_camera->Pan(
                moveX * moveSpeed * deltaTime,
                moveY * moveSpeed * deltaTime
            );
        }
    }

}

void Application::Render()
{
    if (m_renderDevice && m_renderer && m_camera)
    {
        m_renderDevice->BeginFrame();

        // Render all loaded meshes
        if (!m_loadedMeshes.empty())
        {
            // Create transform
            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::scale(transform, glm::vec3(30.0f));
            transform = glm::rotate(transform, m_modelRotation, glm::vec3(0.0f, 1.0f, 0.0f));

            // Draw each mesh with camera
            for (auto& mesh : m_loadedMeshes)
            {
                m_renderer->DrawMesh(mesh.get(), transform, m_camera);
            }
        }
        else
        {
            // Fallback: draw the cube if no model loaded
            m_renderer->DrawCube(m_timer.GetDeltaTime());
        }

        m_renderDevice->EndFrame();
    }
}

void Application::UpdateWindowTitle()
{
    // Update title every 0.5 seconds to avoid flickering
    m_titleUpdateTimer += m_timer.GetDeltaTime();

    if (m_titleUpdateTimer >= 0.5f)
    {
        std::wostringstream title;
        title << L"AniEngine | FPS: " << std::fixed << std::setprecision(1)
            << m_timer.GetFPS()
            << L" | Meshes: " << m_loadedMeshes.size();

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