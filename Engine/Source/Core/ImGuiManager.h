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
using AddModelCallback = std::function<void(const std::string& path)>;
using OpenSceneCallback = std::function<void(const std::string& path)>;
using SaveSceneAsCallback = std::function<void(const std::string& path)>;
using SimpleCallback = std::function<void()>;

// Render settings - SINGLE SOURCE OF TRUTH
struct RenderSettings {
    bool wireframeMode = false;
    float ambientIntensity = 0.1f;
    float lightIntensity = 2.0f;
    float lightDirection[3] = { 0.5f, 1.0f, 0.5f };
    float backgroundColor[3] = { 0.1f, 0.1f, 0.15f };
};

// Gizmo operation mode
enum class GizmoMode {
    Translate,
    Rotate,
    Scale
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
        Camera* camera,
        SceneManager* sceneManager,
        RenderSettings& settings
    );
    void Render();

    bool ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool WantCaptureMouse() const;
    bool WantCaptureKeyboard() const;

    // Returns true if gizmo is being used (so camera shouldn't move)
    bool IsUsingGizmo() const { return m_usingGizmo; }

    // Process keyboard shortcuts (call from Application::Update)
    void ProcessShortcuts(SceneManager* sceneManager);

    // Callbacks
    void SetAddModelCallback(AddModelCallback callback) { m_addModelCallback = callback; }
    void SetNewSceneCallback(SimpleCallback callback) { m_newSceneCallback = callback; }
    void SetOpenSceneCallback(OpenSceneCallback callback) { m_openSceneCallback = callback; }
    void SetSaveSceneCallback(SimpleCallback callback) { m_saveSceneCallback = callback; }
    void SetSaveSceneAsCallback(SaveSceneAsCallback callback) { m_saveSceneAsCallback = callback; }
    void SetFrameSceneCallback(SimpleCallback callback) { m_frameSceneCallback = callback; }
    void SetFrameSelectedCallback(SimpleCallback callback) { m_frameSelectedCallback = callback; }

    // Panel visibility
    bool showStatsPanel = true;
    bool showScenePanel = true;
    bool showInspectorPanel = true;
    bool showRenderSettingsPanel = true;
    bool showDemoWindow = false;

    // Gizmo state
    GizmoMode gizmoMode = GizmoMode::Translate;
    bool gizmoLocal = false;  // Local vs world space
    bool gizmoEnabled = true;

private:
    void SetupDocking();
    void DrawMenuBar(SceneManager* sceneManager);
    void DrawStatsPanel(const Timer* timer, const Camera* camera, const Scene* scene);
    void DrawSceneHierarchyPanel(SceneManager* sceneManager);
    void DrawInspectorPanel(SceneManager* sceneManager);
    void DrawRenderSettingsPanel(RenderSettings& settings);
    void DrawGizmoControls();
    void DrawTransformGizmo(Camera* camera, SceneManager* sceneManager);
    void ApplyTheme(int themeIndex);

    void OpenModelFileDialog();
    void OpenSceneFileDialog();
    void SaveSceneFileDialog();

    RenderDevice* m_device;
    HWND m_hwnd;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    bool m_initialized;
    int m_currentTheme;
    bool m_usingGizmo;  // True when user is dragging gizmo

    // Callbacks
    AddModelCallback m_addModelCallback;
    SimpleCallback m_newSceneCallback;
    OpenSceneCallback m_openSceneCallback;
    SimpleCallback m_saveSceneCallback;
    SaveSceneAsCallback m_saveSceneAsCallback;
    SimpleCallback m_frameSceneCallback;
    SimpleCallback m_frameSelectedCallback;
};