#include "ImGuiManager.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/ModelLoader.h"
#include "Rendering/Material.h"
#include "Rendering/Camera.h"
#include "Rendering/Mesh.h"
#include "Timer.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <commdlg.h>
#include <shobjidl.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImGuiManager::ImGuiManager()
    : m_device(nullptr)
    , m_initialized(false)
    , m_selectedMaterialIndex(-1)
    , m_selectedInstanceIndex(-1)
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

void ImGuiManager::DrawUI(const Timer* timer, const Camera* camera, LoadedModel* model, RenderSettings& settings) {
    if (!m_initialized) {
        return;
    }

    SetupDocking();
    DrawMenuBar();

    if (showStatsPanel) {
        DrawStatsPanel(timer, camera, model);
    }
    if (showScenePanel) {
        DrawSceneHierarchyPanel(model);
    }
    if (showMaterialPanel) {
        DrawMaterialInspector(model);
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

void ImGuiManager::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load Model...", "Ctrl+O")) {
                OpenFileDialog();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                PostQuitMessage(0);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Stats", nullptr, &showStatsPanel);
            ImGui::MenuItem("Scene Hierarchy", nullptr, &showScenePanel);
            ImGui::MenuItem("Material Inspector", nullptr, &showMaterialPanel);
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

        ImGui::EndMainMenuBar();
    }
}

void ImGuiManager::OpenFileDialog() {
    OPENFILENAMEA ofn = {};
    char filename[MAX_PATH] = "";

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "glTF Files (*.gltf;*.glb)\0*.gltf;*.glb\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Load Model";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        if (m_modelLoadCallback) {
            m_modelLoadCallback(std::string(filename));
        }
    }
}

void ImGuiManager::DrawStatsPanel(const Timer* timer, const Camera* camera, const LoadedModel* model) {
    ImGui::Begin("Stats", &showStatsPanel);

    ImGui::Text("Performance");
    ImGui::Separator();
    ImGui::Text("FPS: %.1f", timer ? timer->GetFPS() : 0.0f);
    ImGui::Text("Frame Time: %.3f ms", timer ? timer->GetDeltaTime() * 1000.0f : 0.0f);

    ImGui::Spacing();

    ImGui::Text("Scene");
    ImGui::Separator();
    if (model) {
        ImGui::Text("Mesh Instances: %zu", model->meshInstances.size());
        ImGui::Text("Unique Meshes: %zu", model->meshes.size());
        ImGui::Text("Materials: %zu", model->materials.size());
        ImGui::Text("Textures: %zu", model->textures.size());
    }
    else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No model loaded");
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

void ImGuiManager::DrawSceneHierarchyPanel(LoadedModel* model) {
    ImGui::Begin("Scene Hierarchy", &showScenePanel);

    if (!model || model->meshInstances.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No model loaded");
        ImGui::End();
        return;
    }

    ImGui::Text("Instances: %zu", model->meshInstances.size());
    ImGui::Separator();

    for (size_t i = 0; i < model->meshInstances.size(); ++i) {
        const auto& instance = model->meshInstances[i];

        std::string name = instance.mesh ? instance.mesh->GetName() : "Unnamed";
        if (name.empty()) {
            name = "Mesh " + std::to_string(i);
        }

        bool isSelected = (m_selectedInstanceIndex == static_cast<int>(i));

        if (ImGui::Selectable(name.c_str(), isSelected)) {
            m_selectedInstanceIndex = static_cast<int>(i);

            if (instance.mesh && instance.mesh->GetMaterial()) {
                for (size_t j = 0; j < model->materials.size(); ++j) {
                    if (model->materials[j].get() == instance.mesh->GetMaterial().get()) {
                        m_selectedMaterialIndex = static_cast<int>(j);
                        break;
                    }
                }
            }
        }
    }

    if (m_selectedInstanceIndex >= 0 && m_selectedInstanceIndex < static_cast<int>(model->meshInstances.size())) {
        ImGui::Separator();
        ImGui::Text("Selected Instance");

        const auto& instance = model->meshInstances[m_selectedInstanceIndex];

        glm::vec3 position(
            instance.transform[3][0],
            instance.transform[3][1],
            instance.transform[3][2]
        );

        ImGui::Text("Position: (%.2f, %.2f, %.2f)", position.x, position.y, position.z);

        if (instance.mesh) {
            ImGui::Text("Vertices: %u", instance.mesh->GetVertexCount());
            ImGui::Text("Triangles: %u", instance.mesh->GetIndexCount() / 3);
        }
    }

    ImGui::End();
}

void ImGuiManager::DrawMaterialInspector(LoadedModel* model) {
    ImGui::Begin("Material Inspector", &showMaterialPanel);

    if (!model || model->materials.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No materials");
        ImGui::End();
        return;
    }

    std::vector<const char*> materialNames;
    for (const auto& mat : model->materials) {
        materialNames.push_back(mat->name.empty() ? "Unnamed" : mat->name.c_str());
    }

    if (m_selectedMaterialIndex < 0) {
        m_selectedMaterialIndex = 0;
    }

    ImGui::Combo("Material", &m_selectedMaterialIndex, materialNames.data(), static_cast<int>(materialNames.size()));
    ImGui::Separator();

    if (m_selectedMaterialIndex >= 0 && m_selectedMaterialIndex < static_cast<int>(model->materials.size())) {
        auto& material = model->materials[m_selectedMaterialIndex];

        ImGui::Text("Base Color");
        ImGui::ColorEdit4("##baseColor", &material->baseColorFactor.x);
        ImGui::SameLine();
        ImGui::TextColored(
            material->baseColorTexture ? ImVec4(0.4f, 0.8f, 0.4f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            material->baseColorTexture ? "[Textured]" : "[No Texture]"
        );

        ImGui::Spacing();

        ImGui::Text("PBR Properties");
        ImGui::SliderFloat("Metallic", &material->metallicFactor, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness", &material->roughnessFactor, 0.0f, 1.0f);

        ImGui::Spacing();

        ImGui::Text("Normal Map");
        ImGui::SliderFloat("Normal Scale", &material->normalScale, 0.0f, 2.0f);
        ImGui::SameLine();
        ImGui::TextColored(
            material->normalTexture ? ImVec4(0.4f, 0.8f, 0.4f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            material->normalTexture ? "[Has Normal]" : "[No Normal]"
        );

        ImGui::Spacing();

        ImGui::Text("Ambient Occlusion");
        ImGui::SliderFloat("AO Strength", &material->occlusionStrength, 0.0f, 1.0f);
        ImGui::SameLine();
        ImGui::TextColored(
            material->occlusionTexture ? ImVec4(0.4f, 0.8f, 0.4f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            material->occlusionTexture ? "[Has AO]" : "[No AO]"
        );

        ImGui::Spacing();

        ImGui::Text("Emissive");
        ImGui::ColorEdit3("##emissive", &material->emissiveFactor.x);
        ImGui::SameLine();
        ImGui::TextColored(
            material->emissiveTexture ? ImVec4(0.4f, 0.8f, 0.4f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            material->emissiveTexture ? "[Has Emissive]" : "[No Emissive]"
        );

        ImGui::Spacing();

        ImGui::Text("Alpha");
        ImGui::SliderFloat("Cutoff", &material->alphaCutoff, 0.0f, 1.0f);
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