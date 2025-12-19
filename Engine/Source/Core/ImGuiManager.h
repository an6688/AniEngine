#pragma once

#include <d3d12.h>
#include <wrl/client.h>

// Forward declarations
class RenderDevice;
struct HWND__;
typedef HWND__* HWND;

// ImGui integration manager
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

    // Call at the end of rendering (before EndFrame/Present)
    void Render();

    // Forward Windows messages to ImGui (call from Window message handler)
    // Returns true if ImGui consumed the message
    bool ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Check if ImGui wants to capture input (mouse over UI, typing in text field, etc.)
    bool WantCaptureMouse() const;
    bool WantCaptureKeyboard() const;

private:
    RenderDevice* m_device;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    bool m_initialized;
};