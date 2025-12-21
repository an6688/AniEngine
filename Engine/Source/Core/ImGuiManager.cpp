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
#include <commdlg.h>
#include <glm/gtc/type_ptr.hpp>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImGuiManager::ImGuiManager()
    : m_device(nullptr)
    , m_initialized(false)
    , m_currentTheme(0) {
}

ImGuiManager::~ImGuiManager() {
    Shutdown();
}

bool ImGuiManager::Initialize(HWND hwnd, RenderDevice* device) {
    if (m_initialized) {
        return true;
    }

    m_device = device;

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
}

void ImGuiManager::DrawUI(
    const Timer* timer,
    const Camera* camera,
    SceneManager* sceneManager,
    RenderSettings& settings) {

    if (!m_initialized) {
        return;
    }

    SetupDocking();
    DrawMenuBar(sceneManager);

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
    return ImGui::GetIO().WantCaptureMouse;
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

void ImGuiManager::DrawMenuBar(SceneManager* sceneManager) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                if (m_newSceneCallback) {
                    m_newSceneCallback();
                }
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                OpenSceneFileDialog();
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                if (m_saveSceneCallback) {
                    m_saveSceneCallback();
                }
            }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
                SaveSceneFileDialog();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Add Model...", "Ctrl+M")) {
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
            if (ImGui::MenuItem("Frame Scene", "F")) {
                if (m_frameSceneCallback) {
                    m_frameSceneCallback();
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Stats", nullptr, &showStatsPanel);
            ImGui::MenuItem("Scene Hierarchy", nullptr, &showScenePanel);
            ImGui::MenuItem("Inspector", nullptr, &showInspectorPanel);
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

        // Show scene name and dirty state in menu bar
        if (sceneManager) {
            ImGui::Separator();
            std::string sceneLabel = sceneManager->GetSceneName();
            if (sceneManager->HasUnsavedChanges()) {
                sceneLabel += " *";
            }
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", sceneLabel.c_str());
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

    ImGui::End();
}

void ImGuiManager::DrawSceneHierarchyPanel(SceneManager* sceneManager) {
    ImGui::Begin("Scene Hierarchy", &showScenePanel);

    if (!sceneManager || !sceneManager->GetScene()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No scene");
        ImGui::End();
        return;
    }

    Scene* scene = sceneManager->GetScene();

    // Add button
    if (ImGui::Button("+ Add Model")) {
        OpenModelFileDialog();
    }

    ImGui::Separator();

    // Object list
    for (int i = 0; i < static_cast<int>(scene->objects.size()); ++i) {
        auto& obj = scene->objects[i];

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (scene->selectedObjectIndex == i) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        // Visibility toggle
        ImGui::PushID(i);
        if (ImGui::Checkbox("##visible", &obj->isVisible)) {
            scene->MarkDirty();
        }
        ImGui::SameLine();

        bool nodeOpen = ImGui::TreeNodeEx(obj->name.c_str(), flags);

        if (ImGui::IsItemClicked()) {
            sceneManager->SelectObject(i);
        }

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Duplicate")) {
                sceneManager->DuplicateObject(i);
            }
            if (ImGui::MenuItem("Delete")) {
                sceneManager->RemoveObject(i);
            }
            if (ImGui::MenuItem("Focus", "F")) {
                sceneManager->SelectObject(i);
                if (m_frameSceneCallback) {
                    m_frameSceneCallback();
                }
            }
            ImGui::EndPopup();
        }

        if (nodeOpen) {
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ImGui::End();
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

    if (ImGui::DragFloat3("Position", glm::value_ptr(selected->position), 0.1f)) {
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

    ImGui::Separator();

    // Materials list (expandable)
    if (ImGui::CollapsingHeader("Materials")) {
        for (size_t i = 0; i < selected->materials.size(); ++i) {
            auto& mat = selected->materials[i];
            if (ImGui::TreeNode(mat->name.empty() ? "Unnamed" : mat->name.c_str())) {
                ImGui::ColorEdit4("Base Color", &mat->baseColorFactor.x);
                ImGui::SliderFloat("Metallic", &mat->metallicFactor, 0.0f, 1.0f);
                ImGui::SliderFloat("Roughness", &mat->roughnessFactor, 0.0f, 1.0f);
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

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "glTF Files (*.gltf;*.glb)\0*.gltf;*.glb\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Add Model to Scene";
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

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Open Scene";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        if (m_openSceneCallback) {
            m_openSceneCallback();
        }
    }
}

void ImGuiManager::SaveSceneFileDialog() {
    OPENFILENAMEA ofn = {};
    char filename[MAX_PATH] = "";

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Save Scene As";
    ofn.lpstrDefExt = "scene";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn)) {
        if (m_saveSceneAsCallback) {
            m_saveSceneAsCallback();
        }
    }
}