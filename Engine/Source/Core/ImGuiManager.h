#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <functional>

class RenderDevice;
class Camera;
class Timer;
class SceneManager;
struct Scene;
struct SceneObject;

struct HWND__;
typedef HWND__* HWND;

// Callbacks
using ModelLoadCallback = std::function<void(const std::string& path)>;
using NewSceneCallback = std::function<void()>;
using OpenSceneCallback = std::function<void()>;
using SaveSceneCallback = std::function<void()>;
using SaveSceneAsCallback = std::function<void()>;

// Render settings - SINGLE SOURCE OF TRUTH
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
    void DrawUI(
        const Timer* timer,
        const Camera* camera,
        SceneManager* sceneManager,
        RenderSettings& settings
    );
    void Render();

    bool ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool WantCaptureMouse() const;
    bool WantCaptureKeyboard() const;

    // Callbacks
    void SetAddModelCallback(ModelLoadCallback callback) { m_addModelCallback = callback; }
    void SetNewSceneCallback(NewSceneCallback callback) { m_newSceneCallback = callback; }
    void SetOpenSceneCallback(OpenSceneCallback callback) { m_openSceneCallback = callback; }
    void SetSaveSceneCallback(SaveSceneCallback callback) { m_saveSceneCallback = callback; }
    void SetSaveSceneAsCallback(SaveSceneAsCallback callback) { m_saveSceneAsCallback = callback; }
    void SetFrameSceneCallback(std::function<void()> callback) { m_frameSceneCallback = callback; }

    // Panel visibility
    bool showStatsPanel = true;
    bool showScenePanel = true;
    bool showInspectorPanel = true;
    bool showRenderSettingsPanel = true;
    bool showDemoWindow = false;

private:
    void SetupDocking();
    void DrawMenuBar(SceneManager* sceneManager);
    void DrawStatsPanel(const Timer* timer, const Camera* camera, const Scene* scene);
    void DrawSceneHierarchyPanel(SceneManager* sceneManager);
    void DrawInspectorPanel(SceneManager* sceneManager);
    void DrawRenderSettingsPanel(RenderSettings& settings);
    void ApplyTheme(int themeIndex);

    void OpenModelFileDialog();
    void OpenSceneFileDialog();
    void SaveSceneFileDialog();

    RenderDevice* m_device;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    bool m_initialized;
    int m_currentTheme;

    // Callbacks
    ModelLoadCallback m_addModelCallback;
    NewSceneCallback m_newSceneCallback;
    OpenSceneCallback m_openSceneCallback;
    SaveSceneCallback m_saveSceneCallback;
    SaveSceneAsCallback m_saveSceneAsCallback;
    std::function<void()> m_frameSceneCallback;
};