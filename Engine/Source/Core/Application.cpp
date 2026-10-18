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
#include <filesystem>

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

	m_projectDialog.SetHWND(m_window.GetHandle());

	m_showProjectDialog = true;

	m_textureManager = new TextureManager();
	if (!m_textureManager->Initialize(m_renderDevice))
		return false;

	// Scene manager replaces the one off model load pathway
	if (!m_sceneManager.Initialize(m_renderDevice, m_textureManager))
		return false;

	m_renderer = new Renderer();
	if (!m_renderer->Initialize(m_renderDevice, m_textureManager))
		return false;
    if (!m_imageProcessor.Initialize(m_renderDevice)) {
        MessageBoxA(m_window.GetHandle(), "Image-processing shaders could not be initialized.", "Initialization failed", MB_OK | MB_ICONERROR);
        return false;
    }
	m_renderer->SetRenderSettings(&m_renderSettings);

	m_camera = new Camera();
	m_camera->Initialize(1280.0f / 720.0f);

	m_imguiManager = new ImGuiManager();
	if (!m_imguiManager->Initialize(m_window.GetHandle(), m_renderDevice))
		return false;
	m_imguiManager->SetShadowMap(m_renderer->GetShadowMap());
	m_window.SetImGuiManager(m_imguiManager);

	m_imguiManager->SetProjectManager(&m_projectManager);
	m_imguiManager->SetSceneManager(&m_sceneManager);

	m_imguiManager->SetAddModelCallback([this](const std::string& path) {
		std::string assetPath = path;

		if (m_projectManager.HasOpenProject()) {
			std::string relativePath = m_projectManager.ImportAsset(path);
			if (!relativePath.empty()) {
				assetPath = m_projectManager.GetFullAssetPath(relativePath);
			}
			// If import fails, fall through to use original path
		}

		if (m_sceneManager.AddObjectFromFile(assetPath)) {
			FrameScene();
		}
		});

	m_imguiManager->SetNewSceneCallback([this]() {
		m_sceneManager.NewScene();
		});

    auto rememberScene = [this]() {
        if (auto* project = m_projectManager.GetProject()) {
            project->lastOpenedScene = std::filesystem::path(m_sceneManager.GetScenePath()).stem().string();
            if (!m_projectManager.SaveProject())
                MessageBoxA(m_window.GetHandle(), "Scene saved/opened, but project settings could not be saved.", "Project save failed", MB_OK | MB_ICONERROR);
        }
    };
    m_imguiManager->SetOpenSceneCallback([this, rememberScene](const std::string& path) {
        if (!m_sceneManager.LoadScene(path)) {
            MessageBoxA(m_window.GetHandle(), "The scene or one of its assets could not be loaded. Your current scene was kept.", "Open scene failed", MB_OK | MB_ICONERROR);
            return;
        }
        FrameScene();
        if (m_projectManager.HasOpenProject()) {
            auto destination = m_projectManager.GetFullScenePath(std::filesystem::path(path).stem().string());
            std::error_code error;
            if (!std::filesystem::equivalent(path, destination, error)) {
                // External scenes become project scenes on their next save.
                m_sceneManager.GetScene()->filePath.clear();
                m_sceneManager.GetScene()->MarkDirty();
                m_imguiManager->RequestSceneSave();
                return;
            }
            rememberScene();
        }
    });
    m_imguiManager->SetSaveSceneCallback([this, rememberScene]() {
        if (m_sceneManager.SaveScene()) rememberScene();
        else MessageBoxA(m_window.GetHandle(), "The scene could not be saved.", "Save failed", MB_OK | MB_ICONERROR);
    });
    m_imguiManager->SetSaveSceneAsCallback([this, rememberScene](const std::string& path) {
        // Bring externally referenced models into the project before saving.
        if (auto* project = m_projectManager.GetProject()) {
            for (auto& object : m_sceneManager.GetScene()->objects) {
                std::error_code error;
                auto relative = std::filesystem::relative(object->assetPath, project->GetAssetsPath(), error);
                if (error || relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
                    auto imported = m_projectManager.ImportAsset(object->assetPath);
                    if (imported.empty()) {
                        MessageBoxA(m_window.GetHandle(), "A model could not be imported into the project. The scene was not saved.", "Import failed", MB_OK | MB_ICONERROR);
                        return;
                    }
                    object->assetPath = m_projectManager.GetFullAssetPath(imported);
                }
            }
        }
        if (m_sceneManager.SaveSceneAs(path)) rememberScene();
        else MessageBoxA(m_window.GetHandle(), "The scene could not be saved.", "Save failed", MB_OK | MB_ICONERROR);
    });

	m_imguiManager->SetFrameSceneCallback([this]() {
		FrameScene();
		});

	m_imguiManager->SetFrameSelectedCallback([this]() {
		FrameSelected();
		});

	m_imguiManager->SetShowProjectDialogCallback([this]() {
		m_projectDialog.Reset();
		m_showProjectDialog = true;
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
		m_renderDevice->cpuFrameStart = ProfileNowMs();
		m_input.Update();
		if (!m_window.ProcessMessages())
		{
			m_isRunning = false;
			break;
		}

		m_timer.Tick();

		const double updateStart = ProfileNowMs();
		Update();
		m_renderDevice->cpuUpdateMs = ProfileNowMs() - updateStart;
		Render();
	}
}

void Application::Shutdown()
{
    if (m_renderDevice) m_renderDevice->WaitForGPU();
    m_imageProcessor.Shutdown();
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

	UpdateWindowTitle();
	m_imguiManager->BeginFrame();

	// Show project dialog if needed
	if (m_showProjectDialog) {
		ProjectDialogResult result = m_projectDialog.Show(&m_projectManager);

		switch (result) {
		case ProjectDialogResult::CreateNew:
			if (m_projectManager.CreateProject(
				m_projectDialog.GetNewProjectName(),
				m_projectDialog.GetNewProjectFolder())) {
				m_showProjectDialog = false;
				m_sceneManager.NewScene();
				FrameScene();
			}
			break;

		case ProjectDialogResult::OpenExisting:
		case ProjectDialogResult::OpenRecent:
			if (m_projectManager.OpenProject(m_projectDialog.GetSelectedProjectPath())) {
				m_showProjectDialog = false;

				// Load last scene if available
				const Project* proj = m_projectManager.GetProject();
				if (proj && !proj->lastOpenedScene.empty()) {
					std::string scenePath = m_projectManager.GetFullScenePath(proj->lastOpenedScene);
					if (m_sceneManager.LoadScene(scenePath)) {
						FrameScene();
					}
					else {
						m_sceneManager.NewScene();
					}
				}
				else {
					m_sceneManager.NewScene();
				}
			}
			break;

		case ProjectDialogResult::Cancelled:
			// User closed dialog - allow working without project (optional)
			m_showProjectDialog = false;
			break;

		default:
			// Dialog still open, don't process anything else
			break;
		}

		// Skip rest of update while dialog is showing
		return;
	}

	UpdateCameraInput();
	m_camera->Update(deltaTime);

	// Process keyboard shortcuts
	m_imguiManager->ProcessShortcuts(&m_sceneManager, m_window.IsActive());

	// Handle camera input only if not interacting with UI
	if (!m_imguiManager->WantCaptureMouse() && !m_imguiManager->IsUsingGizmo()) {
		// Viewport picking on left click
		if (m_input.IsMouseButtonPressed(0) && m_window.IsActive() && !m_input.IsKeyDown(VK_MENU)) {
			m_imguiManager->HandleViewportClick(
				m_input.GetMouseX(),
				m_input.GetMouseY(),
				m_camera,
				&m_sceneManager
			);
		}
	}

	// Draw all UI panels
	m_imguiManager->DrawUI(
		&m_timer,
		m_camera,
		&m_sceneManager,
		&m_projectManager,
		m_renderSettings
	);
}


void Application::UpdateCameraInput()
{
	// Prevent camera movement when mouse is over imgui
	if (!m_window.IsActive() || m_showProjectDialog ||
		(m_imguiManager && (m_imguiManager->WantCaptureMouse() || ImGui::GetIO().WantTextInput))) {
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


	if (!altDown && !m_input.IsKeyDown(VK_CONTROL))
	{
		// Pan already scales by camera distance; do not apply it twice.
		float moveSpeed = deltaTime * 20.0f;

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
    if (m_renderSettings.imageProcessing.mode != ImageProcessingSettings::Original &&
        !m_imageProcessor.Prepare(m_renderDevice->GetWidth(), m_renderDevice->GetHeight())) {
        m_renderSettings.imageProcessing.mode = ImageProcessingSettings::Original;
        MessageBoxA(m_window.GetHandle(), "Image-processing resources could not be allocated. Showing original scene.", "Image processing", MB_OK | MB_ICONERROR);
    }
    m_renderDevice->cpuRecordStart = ProfileNowMs();
	m_renderDevice->SetClearColor(
		m_renderSettings.backgroundColor[0],
		m_renderSettings.backgroundColor[1],
		m_renderSettings.backgroundColor[2]
	);

	m_renderDevice->BeginFrame();
	m_renderer->BeginFrame();

	m_renderDevice->Timestamp(0);
	m_renderer->RenderDirectionalShadow(m_sceneManager.GetScene());
	m_renderDevice->Timestamp(1);
	m_renderer->UpdateLightingFromScene(m_sceneManager.GetScene());

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
	m_renderDevice->Timestamp(2);
    m_imageProcessor.Process(m_renderDevice->GetCurrentColorTarget(), m_renderDevice->GetCurrentColorRTV(), m_renderSettings.imageProcessing);
    m_renderDevice->BindMainRenderTargets();
    m_renderDevice->Timestamp(3);
	m_imguiManager->Render();
	m_renderDevice->Timestamp(4);
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