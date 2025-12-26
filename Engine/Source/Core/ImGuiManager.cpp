#include "ImGuiManager.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/Camera.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Timer.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <ImGuizmo.h>
#include <commdlg.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <shellapi.h>
#include <filesystem>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImGuiManager::ImGuiManager()
    : m_device(nullptr)
    , m_hwnd(nullptr)
    , m_initialized(false)
    , m_currentTheme(0)
    , m_usingGizmo(false) {
    memset(m_keyStates, 0, sizeof(m_keyStates));
    memset(m_keyStatesPrev, 0, sizeof(m_keyStatesPrev));
}

ImGuiManager::~ImGuiManager() {
    Shutdown();
}

bool ImGuiManager::Initialize(HWND hwnd, RenderDevice* device) {
    if (m_initialized) {
        return true;
    }

    m_device = device;
    m_hwnd = hwnd;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 64;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = device->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap));
    if (FAILED(hr)) {
        OutputDebugStringA("ImGuiManager: Failed to create descriptor heap\n");
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ApplyTheme(0);

    if (!ImGui_ImplWin32_Init(hwnd)) {
        OutputDebugStringA("ImGuiManager: Failed to initialize Win32 backend\n");
        return false;
    }

    if (!ImGui_ImplDX12_Init(
        device->GetDevice(),
        RenderDevice::FrameBufferCount,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        m_srvHeap.Get(),
        m_srvHeap->GetCPUDescriptorHandleForHeapStart(),
        m_srvHeap->GetGPUDescriptorHandleForHeapStart())) {
        OutputDebugStringA("ImGuiManager: Failed to initialize DX12 backend\n");
        ImGui_ImplWin32_Shutdown();
        return false;
    }

    io.Fonts->Build();
    ImGui_ImplDX12_CreateDeviceObjects();

    m_initialized = true;
    return true;
}

void ImGuiManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    m_srvHeap.Reset();
    m_device = nullptr;
    m_initialized = false;
}

void ImGuiManager::BeginFrame() {
    if (!m_initialized) {
        return;
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void ImGuiManager::ProcessShortcuts(SceneManager* sceneManager, bool windowFocused) {
    if (!m_initialized) {
        return;
    }

    // Save previous key states
    memcpy(m_keyStatesPrev, m_keyStates, sizeof(m_keyStates));

    // Only read key states if window is focused
    if (windowFocused) {
        for (int i = 0; i < 256; i++) {
            m_keyStates[i] = (GetKeyState(i) & 0x8000) != 0;
        }
    }
    else {
        // Window not focused - clear all key states
        memset(m_keyStates, 0, sizeof(m_keyStates));
        return;
    }

    // Don't process shortcuts if ImGui wants keyboard
    if (ImGui::GetIO().WantCaptureKeyboard) {
        return;
    }

    // Helper lambda: key was just pressed this frame
    auto keyPressed = [this](int vk) -> bool {
        return m_keyStates[vk] && !m_keyStatesPrev[vk];
        };

    bool ctrlHeld = m_keyStates[VK_CONTROL];
    bool shiftHeld = m_keyStates[VK_SHIFT];

    // Ctrl+N - New Scene
    if (ctrlHeld && keyPressed('N')) {
        if (m_newSceneCallback) {
            m_newSceneCallback();
        }
    }

    // Ctrl+O - Open Scene
    if (ctrlHeld && keyPressed('O')) {
        OpenSceneFileDialog();
    }

    // Ctrl+S - Save Scene
    if (ctrlHeld && !shiftHeld && keyPressed('S')) {
        if (sceneManager && !sceneManager->GetScenePath().empty()) {
            if (m_saveSceneCallback) {
                m_saveSceneCallback();
            }
        }
        else {
            SaveSceneFileDialog();
        }
    }

    // Ctrl+Shift+S - Save Scene As
    if (ctrlHeld && shiftHeld && keyPressed('S')) {
        SaveSceneFileDialog();
    }

    // Ctrl+M - Add Model
    if (ctrlHeld && keyPressed('M')) {
        OpenModelFileDialog();
    }

    // Ctrl+D - Duplicate
    if (ctrlHeld && keyPressed('D')) {
        if (sceneManager && sceneManager->GetSelectedIndex() >= 0) {
            sceneManager->DuplicateObject(sceneManager->GetSelectedIndex());
        }
    }

    // Delete - Remove selected
    if (keyPressed(VK_DELETE)) {
        if (sceneManager && sceneManager->GetSelectedIndex() >= 0) {
            sceneManager->RemoveObject(sceneManager->GetSelectedIndex());
        }
    }

    // F - Frame scene/selection
    if (keyPressed('F')) {
        if (sceneManager && sceneManager->GetSelectedObject() && m_frameSelectedCallback) {
            m_frameSelectedCallback();
        }
        else if (m_frameSceneCallback) {
            m_frameSceneCallback();
        }
    }

    // W - Translate gizmo
    if (keyPressed('W')) {
        gizmoMode = GizmoMode::Translate;
    }

    // E - Rotate gizmo
    if (keyPressed('E')) {
        gizmoMode = GizmoMode::Rotate;
    }

    // R - Scale gizmo
    if (keyPressed('R')) {
        gizmoMode = GizmoMode::Scale;
    }

    // Q - Toggle gizmo local/world
    if (keyPressed('Q')) {
        gizmoLocal = !gizmoLocal;
    }
}

glm::vec3 ImGuiManager::ScreenToWorldRay(int mouseX, int mouseY, Camera* camera) {
    ImGuiIO& io = ImGui::GetIO();
    float screenWidth = io.DisplaySize.x;
    float screenHeight = io.DisplaySize.y;

    // Convert to normalized device coordinates (-1 to 1)
    float ndcX = (2.0f * mouseX / screenWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY / screenHeight);  // Flip Y

    // Create clip space point on near plane
    glm::vec4 clipNear(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 clipFar(ndcX, ndcY, 1.0f, 1.0f);

    // Get inverse matrices
    glm::mat4 invProj = glm::inverse(camera->GetProjectionMatrix());
    glm::mat4 invView = glm::inverse(camera->GetViewMatrix());

    // Unproject to view space
    glm::vec4 viewNear = invProj * clipNear;
    glm::vec4 viewFar = invProj * clipFar;
    viewNear /= viewNear.w;
    viewFar /= viewFar.w;

    // Unproject to world space
    glm::vec4 worldNear = invView * viewNear;
    glm::vec4 worldFar = invView * viewFar;

    // Calculate ray direction
    glm::vec3 rayDir = glm::normalize(glm::vec3(worldFar) - glm::vec3(worldNear));

    return rayDir;
}

bool ImGuiManager::RayIntersectsAABB(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
    const glm::vec3& boxMin, const glm::vec3& boxMax, float& tOut) {
    float tmin = -FLT_MAX;
    float tmax = FLT_MAX;

    for (int i = 0; i < 3; i++) {
        if (std::abs(rayDir[i]) < 1e-6f) {
            // Ray is parallel to slab
            if (rayOrigin[i] < boxMin[i] || rayOrigin[i] > boxMax[i]) {
                return false;
            }
        }
        else {
            float invD = 1.0f / rayDir[i];
            float t1 = (boxMin[i] - rayOrigin[i]) * invD;
            float t2 = (boxMax[i] - rayOrigin[i]) * invD;

            if (t1 > t2) std::swap(t1, t2);

            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);

            if (tmin > tmax) {
                return false;
            }
        }
    }

    tOut = tmin;
    return tmin >= 0.0f;  // Only count hits in front of camera
}

void ImGuiManager::HandleViewportClick(int mouseX, int mouseY, Camera* camera, SceneManager* sceneManager) {
    if (!camera || !sceneManager || !sceneManager->GetScene()) {
        return;
    }

    Scene* scene = sceneManager->GetScene();
    glm::vec3 rayOrigin = camera->GetPosition();
    glm::vec3 rayDir = ScreenToWorldRay(mouseX, mouseY, camera);

    int closestIndex = -1;
    float closestT = FLT_MAX;

    for (int i = 0; i < static_cast<int>(scene->objects.size()); i++) {
        auto& obj = scene->objects[i];
        if (!obj->isVisible) {
            continue;
        }

        // Transform object bounds to world space
        // For simplicity compute world space AABB from transformed corners
        glm::vec3 corners[8] = {
            { obj->boundsMin.x, obj->boundsMin.y, obj->boundsMin.z },
            { obj->boundsMax.x, obj->boundsMin.y, obj->boundsMin.z },
            { obj->boundsMin.x, obj->boundsMax.y, obj->boundsMin.z },
            { obj->boundsMax.x, obj->boundsMax.y, obj->boundsMin.z },
            { obj->boundsMin.x, obj->boundsMin.y, obj->boundsMax.z },
            { obj->boundsMax.x, obj->boundsMin.y, obj->boundsMax.z },
            { obj->boundsMin.x, obj->boundsMax.y, obj->boundsMax.z },
            { obj->boundsMax.x, obj->boundsMax.y, obj->boundsMax.z },
        };

        glm::vec3 worldMin(FLT_MAX);
        glm::vec3 worldMax(-FLT_MAX);

        for (const auto& corner : corners) {
            glm::vec4 worldCorner = obj->worldMatrix * glm::vec4(corner, 1.0f);
            worldMin = glm::min(worldMin, glm::vec3(worldCorner));
            worldMax = glm::max(worldMax, glm::vec3(worldCorner));
        }

        float t;
        if (RayIntersectsAABB(rayOrigin, rayDir, worldMin, worldMax, t)) {
            if (t < closestT) {
                closestT = t;
                closestIndex = i;
            }
        }
    }

    if (closestIndex >= 0) {
        sceneManager->SelectObject(closestIndex);
    }
    else {
        sceneManager->ClearSelection();
    }
}

void ImGuiManager::DrawAssetBrowserPanel(ProjectManager* projectManager)
{
	ImGui::Begin("Asset Browser", &showAssetBrowser);

	if (!projectManager || !projectManager->HasOpenProject()) {
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No project open");
		ImGui::TextWrapped("Create or open a project to manage assets.");
		ImGui::End();
		return;
	}

	if (ImGui::Button("Import Model...")) {
		OpenModelFileDialog();
	}
	ImGui::SameLine();
	if (ImGui::Button("Refresh")) {
		// Could add a rescan function to ProjectManager
	}

	ImGui::Separator();

	ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Project: %s",
		projectManager->GetProjectPath().c_str());

	ImGui::Separator();

	if (ImGui::CollapsingHeader("Models", ImGuiTreeNodeFlags_DefaultOpen)) {
		auto models = projectManager->GetAssetsByType("model");

		if (models.empty()) {
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  No models imported");
		}

		for (size_t i = 0; i < models.size(); i++) {
			const auto& asset = models[i];

			ImGui::PushID(static_cast<int>(i));

			// Selectable item
			if (ImGui::Selectable(asset.name.c_str())) {
				// Single click could show info
			}

			// Double click to add to scene
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
				if (m_addModelCallback) {
					std::string fullPath = projectManager->GetFullAssetPath(asset.relativePath);
					m_addModelCallback(fullPath);
				}
			}

			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("Path: %s", asset.relativePath.c_str());
				ImGui::Text("Double-click to add to scene");
				ImGui::EndTooltip();
			}

			// Context menu
			if (ImGui::BeginPopupContextItem()) {
				if (ImGui::MenuItem("Add to Scene")) {
					if (m_addModelCallback) {
						std::string fullPath = projectManager->GetFullAssetPath(asset.relativePath);
						m_addModelCallback(fullPath);
					}
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Show in Explorer")) {
					std::string fullPath = projectManager->GetFullAssetPath(asset.relativePath);
					// Open folder in Explorer
					std::string folder = fullPath.substr(0, fullPath.find_last_of("/\\"));
					ShellExecuteA(NULL, "explore", folder.c_str(), NULL, NULL, SW_SHOWNORMAL);
				}
				if (ImGui::MenuItem("Remove from Project")) {
					projectManager->RemoveAsset(asset.relativePath);
				}
				ImGui::EndPopup();
			}

			ImGui::PopID();
		}
	}

	if (ImGui::CollapsingHeader("Textures")) {
		auto textures = projectManager->GetAssetsByType("texture");

		if (textures.empty()) {
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  No textures imported");
		}

		for (const auto& asset : textures) {
			ImGui::BulletText("%s", asset.name.c_str());
		}
	}

	if (ImGui::CollapsingHeader("Scenes", ImGuiTreeNodeFlags_DefaultOpen)) {
		auto scenes = projectManager->GetSceneList();

		if (scenes.empty()) {
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  No scenes saved yet");
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  Use File > Save Scene As");
		}
		else {
			// Get current scene name for highlighting
			std::string currentScene = "";
			if (m_sceneManager) {
				currentScene = m_sceneManager->GetSceneName();
			}

			for (size_t i = 0; i < scenes.size(); i++) {
				const auto& sceneName = scenes[i];

				ImGui::PushID(static_cast<int>(i));

				// Highlight current scene
				bool isCurrent = (sceneName == currentScene);
				if (isCurrent) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
				}

				if (ImGui::Selectable(sceneName.c_str(), isCurrent)) {
					// Single click selects (visual only for now)
				}

				if (isCurrent) {
					ImGui::PopStyleColor();
				}

				// Double-click to load
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
					std::string scenePath = projectManager->GetFullScenePath(sceneName);
					if (m_openSceneCallback) {
						m_openSceneCallback(scenePath);
					}
				}

				// Tooltip
				if (ImGui::IsItemHovered()) {
					ImGui::BeginTooltip();
					ImGui::Text("Double click to open");
					if (isCurrent) {
						ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "(Currently open)");
					}
					ImGui::EndTooltip();
				}

				// Right click context menu
				if (ImGui::BeginPopupContextItem()) {
					if (ImGui::MenuItem("Open")) {
						std::string scenePath = projectManager->GetFullScenePath(sceneName);
						if (m_openSceneCallback) {
							m_openSceneCallback(scenePath);
						}
					}
					if (ImGui::MenuItem("Show in Explorer")) {
						std::string scenesFolder = projectManager->GetProject()->GetScenesPath();
						ShellExecuteA(NULL, "explore", scenesFolder.c_str(), NULL, NULL, SW_SHOWNORMAL);
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Delete", nullptr, false, !isCurrent)) {
						// Don't allow deleting current scene
						std::string scenePath = projectManager->GetFullScenePath(sceneName);
						std::filesystem::remove(scenePath);
					}
					ImGui::EndPopup();
				}

				ImGui::PopID();
			}
		}
	}

	ImGui::End();
}

void ImGuiManager::DrawUI(
    const Timer* timer,
    Camera* camera,
    SceneManager* sceneManager,
    ProjectManager* projectManager,
    RenderSettings& settings) {

    if (!m_initialized) {
        return;
    }

    SetupDocking();
    DrawMenuBar(sceneManager, projectManager);

    const Scene* scene = sceneManager ? sceneManager->GetScene() : nullptr;

    if (showStatsPanel) {
        DrawStatsPanel(timer, camera, scene);
    }
    if (showScenePanel) {
        DrawSceneHierarchyPanel(sceneManager);
    }
    if (showInspectorPanel) {
        DrawInspectorPanel(sceneManager);
    }
    if (showRenderSettingsPanel) {
        DrawRenderSettingsPanel(settings);
    }

    if (showAssetBrowser) {
        DrawAssetBrowserPanel(projectManager);
    }

    // Draw gizmo for selected object
    DrawTransformGizmo(camera, sceneManager);

    if (showDemoWindow) {
        ImGui::ShowDemoWindow(&showDemoWindow);
    }
}

void ImGuiManager::Render() {
    if (!m_initialized || !m_device) {
        return;
    }

    ImGui::Render();

    ID3D12GraphicsCommandList* commandList = m_device->GetCommandList();
    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
    commandList->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

bool ImGuiManager::ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!m_initialized) {
        return false;
    }

    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
        return true;
    }

    return false;
}

bool ImGuiManager::WantCaptureMouse() const {
    if (!m_initialized) {
        return false;
    }
    // Also block mouse if gizmo is being used
    return ImGui::GetIO().WantCaptureMouse || m_usingGizmo;
}

bool ImGuiManager::WantCaptureKeyboard() const {
    if (!m_initialized) {
        return false;
    }
    return ImGui::GetIO().WantCaptureKeyboard;
}

void ImGuiManager::SetupDocking() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();
}

void ImGuiManager::DrawMenuBar(SceneManager* sceneManager, ProjectManager* projectManager) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project...")) {
                if (m_showProjectDialogCallback) {
                    m_showProjectDialogCallback();
                }
            }
            if (ImGui::MenuItem("Open Project...")) {
                if (m_showProjectDialogCallback) {
                    m_showProjectDialogCallback();
                }
            }

            bool hasProject = projectManager && projectManager->HasOpenProject();
            if (ImGui::MenuItem("Close Project", nullptr, false, hasProject)) {
                if (projectManager) {
                    projectManager->CloseProject();
                }
                if (m_showProjectDialogCallback) {
                    m_showProjectDialogCallback();
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                if (m_newSceneCallback) {
                    m_newSceneCallback();
                }
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                OpenSceneFileDialog();
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                if (sceneManager && !sceneManager->GetScenePath().empty()) {
                    if (m_saveSceneCallback) {
                        m_saveSceneCallback();
                    }
                }
                else {
                    SaveSceneFileDialog();
                }
            }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
                SaveSceneFileDialog();
            }

            ImGui::Separator();

            // === ASSET SECTION ===
            if (ImGui::MenuItem("Import Model...", "Ctrl+M")) {
                OpenModelFileDialog();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                PostQuitMessage(0);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            bool hasSelection = sceneManager && sceneManager->GetSelectedObject();

            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, hasSelection)) {
                if (sceneManager) {
                    sceneManager->DuplicateObject(sceneManager->GetSelectedIndex());
                }
            }
            if (ImGui::MenuItem("Delete", "Delete", false, hasSelection)) {
                if (sceneManager) {
                    sceneManager->RemoveObject(sceneManager->GetSelectedIndex());
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Frame All", "F")) {
                if (m_frameSceneCallback) {
                    m_frameSceneCallback();
                }
            }
            if (ImGui::MenuItem("Frame Selected", "F", false, hasSelection)) {
                if (m_frameSelectedCallback) {
                    m_frameSelectedCallback();
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Stats", nullptr, &showStatsPanel);
            ImGui::MenuItem("Scene Hierarchy", nullptr, &showScenePanel);
            ImGui::MenuItem("Inspector", nullptr, &showInspectorPanel);
            ImGui::MenuItem("Asset Browser", nullptr, &showAssetBrowser);
            ImGui::MenuItem("Render Settings", nullptr, &showRenderSettingsPanel);
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo", nullptr, &showDemoWindow);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Theme")) {
            if (ImGui::MenuItem("Dark", nullptr, m_currentTheme == 0)) {
                ApplyTheme(0);
            }
            if (ImGui::MenuItem("Light", nullptr, m_currentTheme == 1)) {
                ApplyTheme(1);
            }
            if (ImGui::MenuItem("Classic", nullptr, m_currentTheme == 2)) {
                ApplyTheme(2);
            }
            ImGui::EndMenu();
        }

        // Show project and scene info in menu bar
        ImGui::Separator();

        if (projectManager && projectManager->HasOpenProject()) {
            ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Project:");
            ImGui::SameLine(0, 4);
            ImGui::Text("%s", projectManager->GetProjectName().c_str());
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "|");
            ImGui::SameLine();
        }

        if (sceneManager) {
            ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Scene:");
            ImGui::SameLine(0, 4);

            std::string sceneName = sceneManager->GetSceneName();
            if (sceneManager->HasUnsavedChanges()) {
                sceneName += " *";
            }
            ImGui::Text("%s", sceneName.c_str());
        }

        ImGui::EndMainMenuBar();
    }
}

void ImGuiManager::DrawStatsPanel(const Timer* timer, const Camera* camera, const Scene* scene) {
    ImGui::Begin("Stats", &showStatsPanel);

    ImGui::Text("Performance");
    ImGui::Separator();
    ImGui::Text("FPS: %.1f", timer ? timer->GetFPS() : 0.0f);
    ImGui::Text("Frame Time: %.3f ms", timer ? timer->GetDeltaTime() * 1000.0f : 0.0f);

    ImGui::Spacing();

    ImGui::Text("Scene");
    ImGui::Separator();
    if (scene) {
        size_t totalMeshInstances = 0;
        size_t totalMaterials = 0;
        for (const auto& obj : scene->objects) {
            totalMeshInstances += obj->meshInstances.size();
            totalMaterials += obj->materials.size();
        }
        ImGui::Text("Objects: %zu", scene->objects.size());
        ImGui::Text("Mesh Instances: %zu", totalMeshInstances);
        ImGui::Text("Materials: %zu", totalMaterials);
    }
    else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No scene");
    }

    ImGui::Spacing();

    ImGui::Text("Camera");
    ImGui::Separator();
    if (camera) {
        glm::vec3 pos = camera->GetPosition();
        glm::vec3 target = camera->GetTarget();
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
        ImGui::Text("Target: (%.2f, %.2f, %.2f)", target.x, target.y, target.z);
        ImGui::Text("Distance: %.2f", camera->GetDistance());
    }

    ImGui::Spacing();

    // Gizmo controls
    DrawGizmoControls();

    ImGui::End();
}

void ImGuiManager::DrawGizmoControls() {
    ImGui::Text("Transform Tool");
    ImGui::Separator();

    ImGui::Checkbox("Enable Gizmo", &gizmoEnabled);

    if (ImGui::RadioButton("Translate (W)", gizmoMode == GizmoMode::Translate)) {
        gizmoMode = GizmoMode::Translate;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate (E)", gizmoMode == GizmoMode::Rotate)) {
        gizmoMode = GizmoMode::Rotate;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale (R)", gizmoMode == GizmoMode::Scale)) {
        gizmoMode = GizmoMode::Scale;
    }

    if (ImGui::Checkbox("Local Space (Q)", &gizmoLocal)) {
        // Toggle handled
    }

    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Click in viewport to select objects");

    if (m_usingGizmo) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Dragging...");
    }
}

void ImGuiManager::DrawTransformGizmo(Camera* camera, SceneManager* sceneManager) {
    if (!sceneManager || !camera || !gizmoEnabled) {
        m_usingGizmo = false;
        return;
    }

    SceneObject* selected = sceneManager->GetSelectedObject();
    if (!selected) {
        m_usingGizmo = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    // Set up ImGuizmo
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    // Get view and projection matrices
    glm::mat4 view = camera->GetViewMatrix();
    glm::mat4 proj = camera->GetProjectionMatrix();

    // Determine operation
    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    switch (gizmoMode) {
    case GizmoMode::Translate:
        operation = ImGuizmo::TRANSLATE;
        break;
    case GizmoMode::Rotate:
        operation = ImGuizmo::ROTATE;
        break;
    case GizmoMode::Scale:
        operation = ImGuizmo::SCALE;
        break;
    }

    // Determine mode (local or world)
    ImGuizmo::MODE mode = gizmoLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

    // Get the object's transform matrix
    glm::mat4 objectMatrix = selected->worldMatrix;

    // Draw and manipulate gizmo
    bool manipulated = ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(proj),
        operation,
        mode,
        glm::value_ptr(objectMatrix)
    );

    m_usingGizmo = ImGuizmo::IsUsing();

    if (manipulated) {
        // Decompose the matrix back into position, rotation, scale
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;

        glm::decompose(objectMatrix, scale, rotation, translation, skew, perspective);

        // Update the scene object
        selected->position = translation;
        selected->rotation = rotation;
        selected->scale = scale;
        selected->UpdateWorldMatrix();

        // Mark scene as dirty
        sceneManager->GetScene()->MarkDirty();
    }
}

void ImGuiManager::DrawSceneHierarchyPanel(SceneManager* sceneManager) {
    ImGui::Begin("Scene Hierarchy", &showScenePanel);

    if (!sceneManager || !sceneManager->GetScene()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No scene");
        ImGui::End();
        return;
    }

    Scene* scene = sceneManager->GetScene();

    if (ImGui::Button("+ Add Model")) {
        OpenModelFileDialog();
    }

    ImGui::Separator();

    // Track deferred actions to avoid modifying while iterating
    int deleteIndex = -1;
    int duplicateIndex = -1;
    int focusIndex = -1;

    int objectCount = static_cast<int>(scene->objects.size());
    for (int i = 0; i < objectCount; ++i) {
        auto& obj = scene->objects[i];

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (scene->selectedObjectIndex == i) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::PushID(i);
        if (ImGui::Checkbox("##visible", &obj->isVisible)) {
            scene->MarkDirty();
        }
        ImGui::SameLine();

        bool nodeOpen = ImGui::TreeNodeEx(obj->name.c_str(), flags);

        if (ImGui::IsItemClicked()) {
            sceneManager->SelectObject(i);
        }

        // Context menu - defer actions!
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Duplicate")) {
                duplicateIndex = i;
            }
            if (ImGui::MenuItem("Delete")) {
                deleteIndex = i;
            }
            if (ImGui::MenuItem("Focus")) {
                focusIndex = i;
            }
            ImGui::EndPopup();
        }

        if (nodeOpen) {
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ImGui::End();

    // Process deferred actions AFTER the loop
    if (focusIndex >= 0) {
        sceneManager->SelectObject(focusIndex);
        if (m_frameSelectedCallback) {
            m_frameSelectedCallback();
        }
    }
    if (duplicateIndex >= 0) {
        sceneManager->DuplicateObject(duplicateIndex);
    }
    if (deleteIndex >= 0) {
        sceneManager->RemoveObject(deleteIndex);
    }
}

void ImGuiManager::DrawInspectorPanel(SceneManager* sceneManager) {
    ImGui::Begin("Inspector", &showInspectorPanel);

    if (!sceneManager) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No scene");
        ImGui::End();
        return;
    }

    SceneObject* selected = sceneManager->GetSelectedObject();
    if (!selected) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No object selected");
        ImGui::End();
        return;
    }

    Scene* scene = sceneManager->GetScene();

    // Name
    char nameBuf[256];
    strncpy_s(nameBuf, selected->name.c_str(), sizeof(nameBuf));
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        selected->name = nameBuf;
        scene->MarkDirty();
    }

    ImGui::Separator();

    // Transform
    ImGui::Text("Transform");
    bool transformChanged = false;

    float dragSpeed = 0.1f;

    if (ImGui::DragFloat3("Position", glm::value_ptr(selected->position), dragSpeed)) {
        transformChanged = true;
    }

    // Convert quaternion to euler for editing
    glm::vec3 euler = glm::degrees(glm::eulerAngles(selected->rotation));
    if (ImGui::DragFloat3("Rotation", glm::value_ptr(euler), 1.0f)) {
        selected->rotation = glm::quat(glm::radians(euler));
        transformChanged = true;
    }

    if (ImGui::DragFloat3("Scale", glm::value_ptr(selected->scale), 0.01f, 0.001f, 100.0f)) {
        transformChanged = true;
    }

    // Quick scale buttons
    ImGui::Text("Quick Scale:");
    ImGui::SameLine();
    if (ImGui::SmallButton("0.1x")) {
        selected->scale *= 0.1f;
        transformChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("0.5x")) {
        selected->scale *= 0.5f;
        transformChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("2x")) {
        selected->scale *= 2.0f;
        transformChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("10x")) {
        selected->scale *= 10.0f;
        transformChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset")) {
        selected->scale = glm::vec3(1.0f);
        transformChanged = true;
    }

    if (transformChanged) {
        selected->UpdateWorldMatrix();
        scene->MarkDirty();
    }

    ImGui::Separator();

    // Asset info
    ImGui::Text("Asset");
    ImGui::TextWrapped("Path: %s", selected->assetPath.c_str());
    ImGui::Text("Mesh Instances: %zu", selected->meshInstances.size());
    ImGui::Text("Materials: %zu", selected->materials.size());
    ImGui::Text("Textures: %zu", selected->textures.size());

    ImGui::Separator();

    // Bounds
    ImGui::Text("Bounds (Local)");
    ImGui::Text("Min: (%.2f, %.2f, %.2f)", selected->boundsMin.x, selected->boundsMin.y, selected->boundsMin.z);
    ImGui::Text("Max: (%.2f, %.2f, %.2f)", selected->boundsMax.x, selected->boundsMax.y, selected->boundsMax.z);
    glm::vec3 size = selected->boundsMax - selected->boundsMin;
    ImGui::Text("Size: (%.2f, %.2f, %.2f)", size.x, size.y, size.z);

    ImGui::Separator();

    // Materials list (expandable)
    if (ImGui::CollapsingHeader("Materials")) {
        for (size_t i = 0; i < selected->materials.size(); ++i) {
            auto& mat = selected->materials[i];
            std::string matName = mat->name.empty() ? ("Material " + std::to_string(i)) : mat->name;
            if (ImGui::TreeNode(matName.c_str())) {
                ImGui::ColorEdit4("Base Color", &mat->baseColorFactor.x);
                ImGui::SliderFloat("Metallic", &mat->metallicFactor, 0.0f, 1.0f);
                ImGui::SliderFloat("Roughness", &mat->roughnessFactor, 0.0f, 1.0f);
                ImGui::SliderFloat("Normal Scale", &mat->normalScale, 0.0f, 2.0f);
                ImGui::SliderFloat("Occlusion", &mat->occlusionStrength, 0.0f, 1.0f);
                ImGui::ColorEdit3("Emissive", &mat->emissiveFactor.x);
                ImGui::TreePop();
            }
        }
    }

    ImGui::End();
}

void ImGuiManager::DrawRenderSettingsPanel(RenderSettings& settings) {
    ImGui::Begin("Render Settings", &showRenderSettingsPanel);

    ImGui::Text("Render Mode");
    ImGui::Separator();
    ImGui::Checkbox("Wireframe", &settings.wireframeMode);

    ImGui::Spacing();

    ImGui::Text("Lighting");
    ImGui::Separator();
    ImGui::SliderFloat("Ambient", &settings.ambientIntensity, 0.0f, 1.0f);
    ImGui::SliderFloat("Light Intensity", &settings.lightIntensity, 0.0f, 5.0f);
    ImGui::SliderFloat3("Light Direction", settings.lightDirection, -1.0f, 1.0f);

    // Normalize light direction button
    if (ImGui::Button("Normalize Direction")) {
        glm::vec3 dir(settings.lightDirection[0], settings.lightDirection[1], settings.lightDirection[2]);
        dir = glm::normalize(dir);
        settings.lightDirection[0] = dir.x;
        settings.lightDirection[1] = dir.y;
        settings.lightDirection[2] = dir.z;
    }

    ImGui::Spacing();

    ImGui::Text("Background");
    ImGui::Separator();
    ImGui::ColorEdit3("Clear Color", settings.backgroundColor);

    ImGui::End();
}

void ImGuiManager::ApplyTheme(int themeIndex) {
    m_currentTheme = themeIndex;

    switch (themeIndex) {
    case 0:
        ImGui::StyleColorsDark();
        break;
    case 1:
        ImGui::StyleColorsLight();
        break;
    case 2:
        ImGui::StyleColorsClassic();
        break;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
}

void ImGuiManager::OpenModelFileDialog() {
    OPENFILENAMEA ofn = {};
    char filename[MAX_PATH] = "";
    char initialDir[MAX_PATH] = "";

    // Default to project's Assets/Models folder if project is open
    if (m_projectManager && m_projectManager->HasOpenProject()) {
        std::string modelsPath = m_projectManager->GetProject()->GetAssetsPath() + "/Models";
        strncpy_s(initialDir, modelsPath.c_str(), MAX_PATH - 1);
    }

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = "glTF Files (*.gltf;*.glb)\0*.gltf;*.glb\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = initialDir[0] ? initialDir : nullptr;
    ofn.lpstrTitle = "Import Model";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        if (m_addModelCallback) {
            m_addModelCallback(std::string(filename));
        }
    }
}

void ImGuiManager::OpenSceneFileDialog() {
    OPENFILENAMEA ofn = {};
    char filename[MAX_PATH] = "";
    char initialDir[MAX_PATH] = "";

    // Default to project's Scenes folder if project is open
    if (m_projectManager && m_projectManager->HasOpenProject()) {
        std::string scenesPath = m_projectManager->GetProject()->GetScenesPath();
        strncpy_s(initialDir, scenesPath.c_str(), MAX_PATH - 1);
    }

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = "Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = initialDir[0] ? initialDir : nullptr;
    ofn.lpstrTitle = "Open Scene";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        if (m_openSceneCallback) {
            m_openSceneCallback(std::string(filename));
        }
    }
}

void ImGuiManager::SaveSceneFileDialog() {
    OPENFILENAMEA ofn = {};
    char filename[MAX_PATH] = "";
    char initialDir[MAX_PATH] = "";

    // Default to project's Scenes folder if project is open
    if (m_projectManager && m_projectManager->HasOpenProject()) {
        std::string scenesPath = m_projectManager->GetProject()->GetScenesPath();
        strncpy_s(initialDir, scenesPath.c_str(), MAX_PATH - 1);
    }

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = "Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = initialDir[0] ? initialDir : nullptr;
    ofn.lpstrTitle = "Save Scene As";
    ofn.lpstrDefExt = "scene";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn)) {
        if (m_saveSceneAsCallback) {
            m_saveSceneAsCallback(std::string(filename));
        }
    }
}