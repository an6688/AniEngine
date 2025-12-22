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
#include "ImGuiManager.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>

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
	if (!m_window.Initialize(L"AniEngine", 1280, 720))
		return false;

	m_input.Initialize();
	m_window.SetInputHandler(&m_input);

	m_renderDevice = new RenderDevice();
	if (!m_renderDevice->Initialize(m_window.GetHandle(), m_window.GetWidth(), m_window.GetHeight()))
		return false;

	m_textureManager = new TextureManager();
	if (!m_textureManager->Initialize(m_renderDevice))
		return false;

	// Scene manager replaces the one off model load pathway
	if (!m_sceneManager.Initialize(m_renderDevice, m_textureManager))
		return false;

	m_renderer = new Renderer();
	if (!m_renderer->Initialize(m_renderDevice, m_textureManager))
		return false;
	m_renderer->SetRenderSettings(&m_renderSettings);

	m_camera = new Camera();
	m_camera->Initialize(1280.0f / 720.0f);

	m_imguiManager = new ImGuiManager();
	if (!m_imguiManager->Initialize(m_window.GetHandle(), m_renderDevice))
		return false;
	m_window.SetImGuiManager(m_imguiManager);

	m_imguiManager->SetAddModelCallback([this](const std::string& path) {
		m_sceneManager.AddObjectFromFile(path);
		FrameScene();
		});

	m_imguiManager->SetNewSceneCallback([this]() {
		m_sceneManager.NewScene();
		});

	m_imguiManager->SetOpenSceneCallback([this](const std::string& path) {
		m_sceneManager.LoadScene(path);
		FrameScene();
		});


	m_imguiManager->SetSaveSceneCallback([this]() {
		(void)m_sceneManager.SaveScene();
		});

	m_imguiManager->SetSaveSceneAsCallback([this](const std::string& path) {
		m_sceneManager.SaveSceneAs(path);
		});

	m_imguiManager->SetFrameSceneCallback([this]() {
		FrameScene();
		});

	m_imguiManager->SetFrameSelectedCallback([this]() {
		FrameSelected();
		});

	// Resize callback
	m_window.SetResizeCallback([this](int width, int height) {
		m_resizePending = true;
		m_pendingWidth = width;
		m_pendingHeight = height;
		});
	
	m_timer.Start();
	m_timer.Tick();
	m_isRunning = true;
	return true;
}

void Application::FrameScene()
{
	Scene* scene = m_sceneManager.GetScene();
	if (!scene || scene->objects.empty()) {
		return;
	}

	glm::vec3 boundsMin, boundsMax;
	scene->GetWorldBounds(boundsMin, boundsMax);

	glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
	float radius = glm::length(boundsMax - boundsMin) * 0.5f;

	m_camera->SetTarget(center);
	m_camera->SetDistance(radius * 2.5f);
}

void Application::FrameSelected()
{
	SceneObject* selected = m_sceneManager.GetSelectedObject();
	if (!selected) {
		return;
	}

	// Calculate world-space bounds for this object
	glm::vec3 boundsMin = glm::vec3(FLT_MAX);
	glm::vec3 boundsMax = glm::vec3(-FLT_MAX);

	glm::vec3 localCorners[8] = {
		{ selected->boundsMin.x, selected->boundsMin.y, selected->boundsMin.z },
		{ selected->boundsMax.x, selected->boundsMin.y, selected->boundsMin.z },
		{ selected->boundsMin.x, selected->boundsMax.y, selected->boundsMin.z },
		{ selected->boundsMax.x, selected->boundsMax.y, selected->boundsMin.z },
		{ selected->boundsMin.x, selected->boundsMin.y, selected->boundsMax.z },
		{ selected->boundsMax.x, selected->boundsMin.y, selected->boundsMax.z },
		{ selected->boundsMin.x, selected->boundsMax.y, selected->boundsMax.z },
		{ selected->boundsMax.x, selected->boundsMax.y, selected->boundsMax.z },
	};

	for (const auto& corner : localCorners) {
		glm::vec4 worldCorner = selected->worldMatrix * glm::vec4(corner, 1.0f);
		boundsMin = glm::min(boundsMin, glm::vec3(worldCorner));
		boundsMax = glm::max(boundsMax, glm::vec3(worldCorner));
	}

	glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
	float radius = glm::length(boundsMax - boundsMin) * 0.5f;

	m_camera->SetTarget(center);
	m_camera->SetDistance(radius * 2.5f);
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

void Application::LoadModel(const std::string& path)
{
	// Clear existing model
	m_loadedModel.meshes.clear();
	m_loadedModel.meshInstances.clear();
	m_loadedModel.materials.clear();
	m_loadedModel.textures.clear();

	ModelLoader loader;
	loader.SetTextureManager(m_textureManager);

	if (loader.LoadGLTF(path.c_str(), m_renderDevice, m_loadedModel))
	{
		char msg[512];
		sprintf_s(msg, "Loaded: %s (%zu instances, %zu materials)\n",
			path.c_str(),
			m_loadedModel.meshInstances.size(),
			m_loadedModel.materials.size());
		OutputDebugStringA(msg);

		FrameScene();
	}
	else
	{
		OutputDebugStringA("Failed to load model\n");
		MessageBoxA(nullptr, "Failed to load model!", "Error", MB_OK | MB_ICONERROR);
	}
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
	m_sceneManager.Shutdown();

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
	// Handle deferred resize
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
	m_imguiManager->ProcessShortcuts(&m_sceneManager);

	// Draw all UI panels (replaces DrawDebugUI)
	m_imguiManager->DrawUI(&m_timer, m_camera, &m_sceneManager, m_renderSettings);
}


void Application::UpdateCameraInput()
{
	// Prevent camera movement when mouse is over imgui
	if (m_imguiManager && m_imguiManager->WantCaptureMouse()) {
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
		FrameScene();
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

void Application::Render() {
	m_renderDevice->SetClearColor(
		m_renderSettings.backgroundColor[0],
		m_renderSettings.backgroundColor[1],
		m_renderSettings.backgroundColor[2]
	);

	m_renderDevice->BeginFrame();
	m_renderer->BeginFrame();

	// Draw all scene objects
	Scene* scene = m_sceneManager.GetScene();
	if (scene) {
		for (const auto& obj : scene->objects) {
			if (!obj->isVisible) {
				continue;
			}

			for (size_t i = 0; i < obj->meshInstances.size(); ++i) {
				const auto& instance = obj->meshInstances[i];
				if (instance.mesh) {
					glm::mat4 worldMatrix = obj->GetMeshWorldMatrix(i);
					m_renderer->DrawMeshTextured(instance.mesh.get(), worldMatrix, m_camera);
				}
			}
		}
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