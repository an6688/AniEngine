#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdint>
#include "FrameProfile.h"

// DirectX 12 rendering device - LOW LEVEL DX12 PLUMBING
// Owns: Device, swap chain, command lists, synchronization, descriptor heaps
// Does NOT own: Pipelines, shaders, geometry, materials
class RenderDevice {
public:
    static const UINT FrameBufferCount = 2;
    static constexpr UINT TimestampCount = 5;

    RenderDevice();
    ~RenderDevice();

    bool Initialize(HWND hwnd, int width, int height);
    void Shutdown();

    FrameProfile profile;
    bool vsync = true;
    bool SupportsTearing() const { return m_allowTearing; }
    HRESULT GetLastPresentResult() const { return m_lastPresentResult; }
    UINT GetLastPresentFlags() const { return m_lastPresentFlags; }
    double cpuFrameStart = 0, cpuUpdateMs = 0, cpuRecordStart = 0;
    unsigned shadowDraws = 0, sceneDraws = 0;
    void Timestamp(UINT boundary);
    bool HasGpuTimings() const { return m_timestampHeap != nullptr; }
    UINT GetUsedDescriptors() const { return m_nextSRVIndex; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    void BeginFrame();
    void EndFrame();
    void BindMainRenderTargets();
    ID3D12Resource* GetCurrentColorTarget() const { return m_renderTargets[m_frameIndex].Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentColorRTV() const {
        auto handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += SIZE_T(m_frameIndex) * m_rtvDescriptorSize;
        return handle;
    }
    UINT GetFrameIndex() const { return m_frameIndex; }

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
    bool m_allowTearing = false;
    HRESULT m_lastPresentResult = S_OK;
    UINT m_lastPresentFlags = 0;
    void InitializeTimings();
    void CollectTimings();
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_timestampHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_timestampReadback;
    UINT64 m_timestampFrequency = 0;
    UINT64 m_profileFence[FrameBufferCount] = {};
    FrameSample m_profileSamples[FrameBufferCount];
    unsigned m_profileGeneration[FrameBufferCount] = {};
    UINT m_timestampMask = 0;
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