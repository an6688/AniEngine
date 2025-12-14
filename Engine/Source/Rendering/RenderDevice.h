#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

// DirectX 12 rendering device
// DEPENDS ON: Nothing. Just DX12 and a window handle
class RenderDevice
{
public:
    // Number of back buffers (double buffering)
    static const UINT FrameBufferCount = 2;

    RenderDevice();
    ~RenderDevice();

    // Initialize DirectX 12
    bool Initialize(HWND hwnd, int width, int height);

    // Shutdown and release resources
    void Shutdown();

    // Begin rendering a frame
    void BeginFrame();

    // End frame and present
    void EndFrame();

    // Wait for GPU to finish all work
    void WaitForGPU();

    // Handle window resize
    void OnResize(int width, int height);

    // Get device (for creating resources later)
    ID3D12Device* GetDevice() const { return m_device.Get(); }

    // Get command list (for recording draw commands)
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }

private:
    // Triangle rendering
    bool CreateTriangleResources();
    void RenderTriangle();
    // Helper functions
    bool CreateDevice();
    bool CreateCommandQueue();
    bool CreateSwapChain(HWND hwnd, int width, int height);
    bool CreateRenderTargets();
    bool CreateDepthStencil(int width, int height);
    bool CreateCommandAllocatorsAndList();
    bool CreateFence();

    void MoveToNextFrame();

    // Enable debug layer in debug builds
    void EnableDebugLayer();

private:
    // Core D3D12 objects
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;

    // Command recording
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameBufferCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

    // Render targets (back buffers)
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FrameBufferCount];
    UINT m_rtvDescriptorSize;

    // Depth/stencil
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencil;

    // Synchronization
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValues[FrameBufferCount];
    HANDLE m_fenceEvent;

    // Current frame index
    UINT m_frameIndex;

    // Viewport and scissor rect
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

    // Dimensions
    int m_width;
    int m_height;

    // Triangle resources
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
};