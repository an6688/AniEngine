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
#include <imgui.h>

Application::Application()
	: m_renderDevice(nullptr)
	, m_renderer(nullptr)
	, m_camera(nullptr)
	, m_textureManager(nullptr)
	, m_imguiManager(nullptr)
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
	if (m_imguiManager)
	{
		m_imguiManager->Shutdown();
		delete m_imguiManager;
		m_imguiManager = nullptr;
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

	m_imguiManager = new ImGuiManager();
	if (!m_imguiManager->Initialize(m_window.GetHandle(), m_renderDevice))
	{
		return false;
	}
	m_window.SetImGuiManager(m_imguiManager);

	m_textureManager = new TextureManager();
	if (!m_textureManager->Initialize(m_renderDevice))
	{
		return false;
	}

	m_renderer = new Renderer();
	if (!m_renderer->Initialize(m_renderDevice, m_textureManager))
	{
		return false;
	}

	m_camera = new Camera();
	m_camera->Initialize(1280.0f / 720.0f);
	// Set up resize callback
	m_window.SetResizeCallback([this](int width, int height) {
		m_resizePending = true;
		m_pendingWidth = width;
		m_pendingHeight = height;
		});

	const char* modelPath = "E:/repos/glTF-Sample-Assets-main/glTF-Sample-Assets-main/Models/StainedGlassLamp/glTF/StainedGlassLamp.gltf";

	ModelLoader loader;
	loader.SetTextureManager(m_textureManager);

	if (!loader.LoadGLTF(modelPath, m_renderDevice, m_loadedModel))
	{
		MessageBoxA(nullptr, "Failed to load model! Check the path.", "Warning", MB_OK | MB_ICONWARNING);
	}
	else
	{
		char msg[256];
		sprintf_s(msg, "Loaded %zu mesh instances, %zu materials, %zu textures\n",
			m_loadedModel.meshInstances.size(),
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
	if (m_loadedModel.meshInstances.empty())
	{
		m_camera->FrameBounds(glm::vec3(0.0f), 2.0f);
		m_modelScale = 1.0f;
		m_modelCenter = glm::vec3(0.0f);
		return;
	}

	// Store the scene center and size
	m_modelCenter = m_loadedModel.center;
	float modelRadius = m_loadedModel.size * 0.5f;

	// Don't normalize scale just frame the camera on the actual model
	m_modelScale = 1.0f;

	// Frame camera on the model's actual world space bounds
	m_camera->FrameBounds(m_modelCenter, modelRadius);

	char msg[256];
	sprintf_s(msg, "FrameModel: center(%.2f, %.2f, %.2f), radius: %.2f\n",
		m_modelCenter.x, m_modelCenter.y, m_modelCenter.z, modelRadius);
	OutputDebugStringA(msg);
}

void Application::DrawDebugUI()
{
	// Performance overlay
	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Stats"))
	{
		ImGui::Text("FPS: %.1f", m_timer.GetFPS());
		ImGui::Text("Frame Time: %.3f ms", m_timer.GetDeltaTime() * 1000.0f);
		ImGui::Separator();
		ImGui::Text("Mesh Instances: %zu", m_loadedModel.meshInstances.size());
		ImGui::Text("Materials: %zu", m_loadedModel.materials.size());
		ImGui::Text("Textures: %zu", m_loadedModel.textures.size());
		ImGui::Separator();
		ImGui::Text("Camera Distance: %.2f", m_camera->GetDistance());

		glm::vec3 camPos = m_camera->GetPosition();
		ImGui::Text("Camera Pos: (%.2f, %.2f, %.2f)", camPos.x, camPos.y, camPos.z);
	}
	ImGui::End();

	// This is just generic stuff, add more interesting info like:
	// - Material inspector
	// - Scene hierarchy
	// - Render settings
	// - etc.
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
	if (m_imguiManager)
	{
		m_imguiManager->Shutdown();
		delete m_imguiManager;
		m_imguiManager = nullptr;
	}
	if (m_renderDevice)
	{
		m_renderDevice->Shutdown();
	}

	m_window.Shutdown();
}

void Application::Update()
{
	if (m_resizePending && m_pendingWidth > 0 && m_pendingHeight > 0)
	{
		m_renderDevice->OnResize(m_pendingWidth, m_pendingHeight);
		m_camera->SetAspectRatio(static_cast<float>(m_pendingWidth) / static_cast<float>(m_pendingHeight));
		m_resizePending = false;
	}

	float deltaTime = m_timer.GetDeltaTime();

	UpdateCameraInput();
	m_camera->Update(deltaTime);

	UpdateWindowTitle();
	m_imguiManager->BeginFrame();

	DrawDebugUI();
}


void Application::UpdateCameraInput()
{
	// Prevent camera movement when mouse is over imgui
	if (m_imguiManager->WantCaptureMouse()) {
		return;
	}

	float deltaTime = m_timer.GetDeltaTime();

	// =========================================================================
	// CAMERA CONTROLS (Maya/Blender style):
	//   Alt + Left Mouse:   Orbit (tumble around target)
	//   Alt + Middle Mouse: Pan (slide view)
	//   Alt + Right Mouse:  Zoom (dolly in/out)
	//   Mouse Wheel:        Zoom (quick dolly)
	//   F:                  Frame selected (reset view to model)
	//   WASD:               Pan camera (slide)
	//   QE:                 Move up/down
	// =========================================================================

	bool altDown = m_input.IsKeyDown(VK_MENU);

	// Get mouse deltas
	float deltaX = static_cast<float>(m_input.GetMouseDeltaX());
	float deltaY = static_cast<float>(m_input.GetMouseDeltaY());

	// Alt + Left Mouse: Orbit
	if (altDown && m_input.IsMouseButtonDown(0))
	{
		m_camera->Orbit(deltaX * 0.01f, deltaY * 0.01f);
	}

	// Alt + Middle Mouse: Pan
	if (altDown && m_input.IsMouseButtonDown(2))
	{
		m_camera->Pan(-deltaX, deltaY);
	}

	// Alt + Right Mouse: Zoom (drag up/down)
	if (altDown && m_input.IsMouseButtonDown(1))
	{
		m_camera->Zoom(-deltaY * 0.05f);
	}

	// Mouse wheel: Quick zoom
	int wheelDelta = m_input.GetMouseWheelDelta();
	if (wheelDelta != 0)
	{
		m_camera->Zoom(static_cast<float>(-wheelDelta) * 0.3f);
	}

	// F key: Frame model (with debounce)
	bool fKeyIsDown = m_input.IsKeyDown('F');
	if (fKeyIsDown && !m_fKeyWasDown)
	{
		FrameModel();
	}
	m_fKeyWasDown = fKeyIsDown;

	if (!altDown)  // Only when Alt is not held
	{
		float moveSpeed = m_camera->GetDistance() * deltaTime * 0.5f;

		// Shift for faster movement
		if (m_input.IsKeyDown(VK_SHIFT)) {
			moveSpeed *= 3.0f;
		}
		if (m_input.IsKeyDown('W')) m_camera->PanForward(moveSpeed * 50.0f);
		if (m_input.IsKeyDown('S')) m_camera->PanForward(-moveSpeed * 50.0f);
		if (m_input.IsKeyDown('A')) m_camera->Pan(-moveSpeed * 50.0f, 0.0f);
		if (m_input.IsKeyDown('D')) m_camera->Pan(moveSpeed * 50.0f, 0.0f);
		if (m_input.IsKeyDown('Q')) m_camera->Pan(0.0f, -moveSpeed * 50.0f);
		if (m_input.IsKeyDown('E')) m_camera->Pan(0.0f, moveSpeed * 50.0f);
	}
}

void Application::Render()
{
	m_renderDevice->BeginFrame();

	// Reset the renderer's per frame state (ring buffer offsets)
	m_renderer->BeginFrame();

	if (!m_loadedModel.meshInstances.empty())
	{
		for (const auto& instance : m_loadedModel.meshInstances)
		{
			// Use the instance transform directly it already has the world position
			m_renderer->DrawMeshTextured(instance.mesh.get(), instance.transform, m_camera);
		}
	}
	else if (!m_loadedModel.meshes.empty())
	{
		for (const auto& mesh : m_loadedModel.meshes)
		{
			m_renderer->DrawMeshTextured(mesh.get(), glm::mat4(1.0f), m_camera);
		}
	}
	else
	{
		m_renderer->DrawCube(m_timer.GetDeltaTime());
	}

	m_renderer->EndFrame();
	m_imguiManager->Render();

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
			<< L" | Instances: " << m_loadedModel.meshInstances.size()
			<< L" | Textures: " << m_loadedModel.textures.size()
			<< L" | Dist: " << std::setprecision(2) << m_camera->GetDistance();

		m_window.SetTitle(title.str().c_str());
		m_titleUpdateTimer = 0.0f;
	}
}