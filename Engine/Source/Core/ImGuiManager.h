#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <functional>

// Forward declarations
class RenderDevice;
class Camera;
class Timer;
struct LoadedModel;
struct Material;

struct HWND__;
typedef HWND__* HWND;

// Callback for when user wants to load a new model
using ModelLoadCallback = std::function<void(const std::string& path)>;

// Render settings that can be modified via UI
struct RenderSettings
{
    bool wireframeMode = false;
    bool showNormals = false;
    float ambientIntensity = 0.1f;
    float lightIntensity = 1.0f;
    float lightDirection[3] = { 0.5f, 1.0f, 0.5f };
    float backgroundColor[3] = { 0.65f, 0.68f, 0.7f };
};

// ImGui integration manager with full editor UI
// DEPENDS ON: RenderDevice, Win32 window handle
class ImGuiManager
{
public:
    ImGuiManager();
    ~ImGuiManager();

    // Initialize ImGui with D3D12 backend
    bool Initialize(HWND hwnd, RenderDevice* device);

    // Shutdown and cleanup
    void Shutdown();

    // Call at the start of each frame (after input processing)
    void BeginFrame();

    void DrawUI(
        Timer* timer,
        Camera* camera,
        LoadedModel* model,
        RenderSettings& settings
    );

    // Call at the end of rendering (before EndFrame/Present)
    void Render();

    // Forward Windows messages to ImGui (call from Window message handler)
    bool ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Check if ImGui wants to capture input (mouse over UI, typing in text field, etc.)
    bool WantCaptureMouse() const;
    bool WantCaptureKeyboard() const;

    // Set callback for model loading
    void SetModelLoadCallback(ModelLoadCallback callback) { m_modelLoadCallback = callback; }

    // Panel visibility toggles
    bool showStatsPanel = true;
    bool showScenePanel = true;
    bool showMaterialPanel = true;
    bool showRenderSettingsPanel = true;
    bool showDemoWindow = false;

private:
    void SetupDocking();
    void DrawMenuBar();
    void DrawStatsPanel(Timer* timer, Camera* camera, LoadedModel* model);
    void DrawSceneHierarchyPanel(LoadedModel* model);
    void DrawMaterialInspector(LoadedModel* model);
    void DrawRenderSettingsPanel(RenderSettings& settings);
    void ApplyTheme(int themeIndex);

    void ShowModelLoadDialog();

private:
    RenderDevice* m_device;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    bool m_initialized;

    // UI state
    int m_selectedMaterialIndex;
    int m_selectedInstanceIndex;
    int m_currentTheme;
    bool m_showLoadDialog;
    char m_modelPathBuffer[512];

    // Callbacks
    ModelLoadCallback m_modelLoadCallback;
};