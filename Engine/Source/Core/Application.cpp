#include "Application.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/Renderer.h"
#include "Rendering/Mesh.h"
#include "Rendering/ModelLoader.h"
#include <sstream>
#include <iomanip>
#include <glm/gtc/matrix_transform.hpp>

Application::Application()
    : m_renderDevice(nullptr)
    , m_renderer(nullptr)
    , m_isRunning(false)
    , m_titleUpdateTimer(0.0f)
    , m_modelRotation(0.0f)
{
}

Application::~Application()
{
    Shutdown();

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
        // Process Windows messages
        if (!m_window.ProcessMessages())
        {
            m_isRunning = false;
            break;
        }

        // Update timer
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

    // Update window title with FPS
    UpdateWindowTitle();

    // TODO: Update actual game logic here
}

void Application::Render()
{
    if (m_renderDevice && m_renderer)
    {
        m_renderDevice->BeginFrame();

        // Render all loaded meshes
        if (!m_loadedMeshes.empty())
        {
            // Create transform with scale adjustment
            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::scale(transform, glm::vec3(30.0f));
            transform = glm::rotate(transform, m_modelRotation, glm::vec3(0.0f, 1.0f, 0.0f));

            // Draw each mesh
            for (auto& mesh : m_loadedMeshes)
            {
                m_renderer->DrawMesh(mesh.get(), transform);
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