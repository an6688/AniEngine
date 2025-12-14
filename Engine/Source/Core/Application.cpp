#include "Application.h"
#include "Rendering/RenderDevice.h"
#include <sstream>
#include <iomanip>

Application::Application()
    : m_renderDevice(nullptr)
    , m_isRunning(false)
    , m_titleUpdateTimer(0.0f)
{
}

Application::~Application()
{
    Shutdown();

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
    if (m_renderDevice)
    {
        m_renderDevice->BeginFrame();

        // TODO: Draw calls

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
            << m_timer.GetFPS();

        m_window.SetTitle(title.str().c_str());
        m_titleUpdateTimer = 0.0f;
    }
}