#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdint>

// DirectX 12 rendering device - LOW LEVEL DX12 PLUMBING
// Owns: Device, swap chain, command lists, synchronization, descriptor heaps
// Does NOT own: Pipelines, shaders, geometry, materials
class RenderDevice {
public:
    static const UINT FrameBufferCount = 2;

    RenderDevice();
    ~RenderDevice();

    bool Initialize(HWND hwnd, int width, int height);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    void WaitForGPU();
    void OnResize(int width, int height);

    // Accessors
    ID3D12Device* GetDevice() const { return m_device.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue.Get(); }
    ID3D12CommandAllocator* GetCurrentCommandAllocator() const { return m_commandAllocators[m_frameIndex].Get(); }
    ID3D12DescriptorHeap* GetSRVHeap() const { return m_srvHeap.Get(); }
    uint32_t GetSRVDescriptorSize() const { return m_srvDescriptorSize; }

    // SRV allocation
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateSRV(uint32_t& outIndex);
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle(uint32_t index) const;

    // Clear color - owned and used by RenderDevice in BeginFrame
    void SetClearColor(float r, float g, float b) {
        m_clearColor[0] = r;
        m_clearColor[1] = g;
        m_clearColor[2] = b;
    }

private:
    bool CreateDevice();
    bool CreateCommandQueue();
    bool CreateSwapChain(HWND hwnd, int width, int height);
    bool CreateDescriptorHeaps();
    bool CreateRenderTargetViews();
    bool CreateDepthStencilView(int width, int height);
    bool CreateCommandAllocatorsAndList();
    bool CreateFence();
    bool CreateSRVHeap();

    void MoveToNextFrame();
    void EnableDebugLayer();

private:
    // Core D3D12 objects
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;

    // Command recording
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameBufferCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

    // Render targets
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FrameBufferCount];
    UINT m_rtvDescriptorSize;

    // Depth stencil
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencil;

    // SRV heap for textures
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    uint32_t m_srvDescriptorSize;
    uint32_t m_nextSRVIndex;
    static constexpr uint32_t MaxSRVDescriptors = 1024;

    // Synchronization
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValues[FrameBufferCount];
    HANDLE m_fenceEvent;

    // Current frame
    UINT m_frameIndex;

    // Viewport and scissor
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

    // Dimensions
    int m_width;
    int m_height;

    // Clear color (used in BeginFrame)
    float m_clearColor[4] = { 0.1f, 0.1f, 0.15f, 1.0f };
};