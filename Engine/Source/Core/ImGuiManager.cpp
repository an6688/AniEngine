#include "ImGuiManager.h"
#include "Rendering/RenderDevice.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

// Forward declare the Win32 message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImGuiManager::ImGuiManager()
    : m_device(nullptr)
    , m_initialized(false)
{
}

ImGuiManager::~ImGuiManager()
{
    Shutdown();
}

bool ImGuiManager::Initialize(HWND hwnd, RenderDevice* device)
{
    if (m_initialized) {
        return true;
    }

    m_device = device;

    // Create a dedicated descriptor heap for ImGui
    // ImGui needs its own SRV heap for font texture and any images
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 64;  // Enough for fonts + some images
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = device->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap));
    if (FAILED(hr))
    {
        OutputDebugStringA("ImGuiManager: Failed to create descriptor heap\n");
        return false;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Visual aesthetics for readability or whatever
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;

    // Setup Platform/Renderer backends
    if (!ImGui_ImplWin32_Init(hwnd))
    {
        OutputDebugStringA("ImGuiManager: Failed to initialize Win32 backend\n");
        return false;
    }

    if (!ImGui_ImplDX12_Init(
        device->GetDevice(),
        RenderDevice::FrameBufferCount,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        m_srvHeap.Get(),
        m_srvHeap->GetCPUDescriptorHandleForHeapStart(),
        m_srvHeap->GetGPUDescriptorHandleForHeapStart()))
    {
        OutputDebugStringA("ImGuiManager: Failed to initialize DX12 backend\n");
        ImGui_ImplWin32_Shutdown();
        return false;
    }

    m_initialized = true;
    return true;
}

void ImGuiManager::Shutdown()
{
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

void ImGuiManager::BeginFrame()
{
    if (!m_initialized) {
        return;
    }
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::Render()
{
    if (!m_initialized || !m_device) {
        return;
    }
    // Finalize ImGui rendering
    ImGui::Render();

    // Set ImGui's descriptor heap before rendering
    ID3D12GraphicsCommandList* commandList = m_device->GetCommandList();
    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
    commandList->SetDescriptorHeaps(1, heaps);

    // Render ImGui draw data
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

bool ImGuiManager::ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (!m_initialized) {
        return false;
    }

    // Let ImGui process the message
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
        return true;
    }

    return false;
}

bool ImGuiManager::WantCaptureMouse() const
{
    if (!m_initialized) {
        return false;
    }

    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiManager::WantCaptureKeyboard() const
{
    if (!m_initialized) {
        return false;
    }
        
    return ImGui::GetIO().WantCaptureKeyboard;
}