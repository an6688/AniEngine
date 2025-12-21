#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <functional>

class RenderDevice;
class Camera;
class Timer;
struct LoadedModel;

struct HWND__;
typedef HWND__* HWND;

using ModelLoadCallback = std::function<void(const std::string& path)>;

// Render settings - SINGLE SOURCE OF TRUTH
// Owned by Application, passed to Renderer and ImGuiManager
struct RenderSettings {
    bool wireframeMode = false;
    float ambientIntensity = 0.1f;
    float lightIntensity = 2.0f;
    float lightDirection[3] = { 0.5f, 1.0f, 0.5f };
    float backgroundColor[3] = { 0.1f, 0.1f, 0.15f };
};

class ImGuiManager {
public:
    ImGuiManager();
    ~ImGuiManager();

    bool Initialize(HWND hwnd, RenderDevice* device);
    void Shutdown();

    void BeginFrame();
    void DrawUI(const Timer* timer, const Camera* camera, LoadedModel* model, RenderSettings& settings);
    void Render();

    bool ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool WantCaptureMouse() const;
    bool WantCaptureKeyboard() const;

    void SetModelLoadCallback(ModelLoadCallback callback) { m_modelLoadCallback = callback; }

    bool showStatsPanel = true;
    bool showScenePanel = true;
    bool showMaterialPanel = true;
    bool showRenderSettingsPanel = true;
    bool showDemoWindow = false;

private:
    void SetupDocking();
    void DrawMenuBar();
    void DrawStatsPanel(const Timer* timer, const Camera* camera, const LoadedModel* model);
    void DrawSceneHierarchyPanel(LoadedModel* model);
    void DrawMaterialInspector(LoadedModel* model);
    void DrawRenderSettingsPanel(RenderSettings& settings);
    void ApplyTheme(int themeIndex);
    void OpenFileDialog();

    RenderDevice* m_device;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    bool m_initialized;
    int m_selectedMaterialIndex;
    int m_selectedInstanceIndex;
    int m_currentTheme;
    ModelLoadCallback m_modelLoadCallback;
};