#include "RenderDevice.h"
#include <d3dx12/d3dx12.h>
#include <cassert>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

RenderDevice::RenderDevice()
    : m_frameIndex(0)
    , m_rtvDescriptorSize(0)
    , m_fenceEvent(nullptr)
    , m_width(0)
    , m_height(0)
    , m_srvDescriptorSize(0)
    , m_nextSRVIndex(0) {
    for (UINT i = 0; i < FrameBufferCount; ++i) {
        m_fenceValues[i] = 0;
    }
}

RenderDevice::~RenderDevice() {
    Shutdown();
}

bool RenderDevice::Initialize(HWND hwnd, int width, int height) {
    m_width = width;
    m_height = height;

    EnableDebugLayer();

    if (!CreateDevice()) {
        return false;
    }
    if (!CreateCommandQueue()) {
        return false;
    }
    if (!CreateSwapChain(hwnd, width, height)) {
        return false;
    }
    if (!CreateDescriptorHeaps()) {
        return false;
    }
    if (!CreateRenderTargetViews()) {
        return false;
    }
    if (!CreateDepthStencilView(width, height)) {
        return false;
    }
    if (!CreateCommandAllocatorsAndList()) {
        return false;
    }
    if (!CreateFence()) {
        return false;
    }
    if (!CreateSRVHeap()) {
        return false;
    }

    m_viewport.TopLeftX = 0.0f;
    m_viewport.TopLeftY = 0.0f;
    m_viewport.Width = static_cast<float>(width);
    m_viewport.Height = static_cast<float>(height);
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    m_scissorRect.left = 0;
    m_scissorRect.top = 0;
    m_scissorRect.right = width;
    m_scissorRect.bottom = height;

    return true;
}

void RenderDevice::Shutdown() {
    WaitForGPU();

    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
}

void RenderDevice::BeginFrame() {
    m_commandAllocators[m_frameIndex]->Reset();
    m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    m_commandList->ResourceBarrier(1, &barrier);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        m_frameIndex,
        m_rtvDescriptorSize
    );

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(
        m_dsvHeap->GetCPUDescriptorHandleForHeapStart()
    );

    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    m_commandList->ClearRenderTargetView(rtvHandle, m_clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);
}

void RenderDevice::EndFrame() {
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    m_commandList->ResourceBarrier(1, &barrier);

    HRESULT hr = m_commandList->Close();
    if (FAILED(hr)) {
        char msg[128];
        sprintf_s(msg, "CommandList Close FAILED hr=0x%08X\n", hr);
        OutputDebugStringA(msg);
    }

    ID3D12CommandList* commandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, commandLists);

    hr = m_swapChain->Present(1, 0);
    if (FAILED(hr)) {
        char msg[128];
        sprintf_s(msg, "Present FAILED hr=0x%08X\n", hr);
        OutputDebugStringA(msg);

        if (hr == DXGI_ERROR_DEVICE_REMOVED) {
            HRESULT reason = m_device->GetDeviceRemovedReason();
            sprintf_s(msg, "Device removed reason: 0x%08X\n", reason);
            OutputDebugStringA(msg);
        }
    }

    MoveToNextFrame();
}

void RenderDevice::WaitForGPU() {
    if (!m_commandQueue || !m_fence || !m_fenceEvent) {
        return;
    }

    const UINT64 fenceValue = m_fenceValues[m_frameIndex];
    HRESULT hr = m_commandQueue->Signal(m_fence.Get(), fenceValue);
    if (FAILED(hr)) {
        return;
    }

    if (m_fence->GetCompletedValue() < fenceValue) {
        hr = m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
        if (SUCCEEDED(hr)) {
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }

    m_fenceValues[m_frameIndex]++;
}

void RenderDevice::OnResize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }

    WaitForGPU();

    for (UINT i = 0; i < FrameBufferCount; ++i) {
        m_renderTargets[i].Reset();
    }
    m_depthStencil.Reset();

    DXGI_SWAP_CHAIN_DESC desc = {};
    m_swapChain->GetDesc(&desc);
    HRESULT hr = m_swapChain->ResizeBuffers(
        FrameBufferCount,
        width,
        height,
        desc.BufferDesc.Format,
        desc.Flags
    );

    if (FAILED(hr)) {
        char msg[128];
        sprintf_s(msg, "OnResize: ResizeBuffers FAILED hr=0x%08X\n", hr);
        OutputDebugStringA(msg);
        return;
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    UINT64 nextFenceValue = m_fenceValues[m_frameIndex] + 1;
    for (UINT i = 0; i < FrameBufferCount; ++i) {
        m_fenceValues[i] = nextFenceValue;
    }

    CreateRenderTargetViews();
    CreateDepthStencilView(width, height);

    m_viewport.Width = static_cast<float>(width);
    m_viewport.Height = static_cast<float>(height);

    m_scissorRect.right = width;
    m_scissorRect.bottom = height;

    m_width = width;
    m_height = height;
}

void RenderDevice::EnableDebugLayer() {
#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }
#endif
}

bool RenderDevice::CreateDevice() {
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        hr = D3D12CreateDevice(
            adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        );

        if (SUCCEEDED(hr)) {
            break;
        }
    }

    if (!m_device) {
        factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
        hr = D3D12CreateDevice(
            adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        );
    }

    return SUCCEEDED(hr);
}

bool RenderDevice::CreateCommandQueue() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    HRESULT hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
    return SUCCEEDED(hr);
}

bool RenderDevice::CreateSwapChain(HWND hwnd, int width, int height) {
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = FrameBufferCount;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
    hr = factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );

    if (FAILED(hr)) {
        return false;
    }

    hr = swapChain1.As(&m_swapChain);
    if (FAILED(hr)) {
        return false;
    }

    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    return true;
}

bool RenderDevice::CreateDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));
    if (FAILED(hr)) {
        return false;
    }

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    hr = m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap));
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

bool RenderDevice::CreateRenderTargetViews() {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

    for (UINT i = 0; i < FrameBufferCount; ++i) {
        HRESULT hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
        if (FAILED(hr)) {
            return false;
        }

        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);
    }

    return true;
}

bool RenderDevice::CreateDepthStencilView(int width, int height) {
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    CD3DX12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT,
        width,
        height,
        1,
        1
    );
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;

    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&m_depthStencil)
    );
    if (FAILED(hr)) {
        return false;
    }

    m_device->CreateDepthStencilView(
        m_depthStencil.Get(),
        nullptr,
        m_dsvHeap->GetCPUDescriptorHandleForHeapStart()
    );

    return true;
}

bool RenderDevice::CreateCommandAllocatorsAndList() {
    for (UINT i = 0; i < FrameBufferCount; ++i) {
        HRESULT hr = m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_commandAllocators[i])
        );
        if (FAILED(hr)) {
            return false;
        }
    }

    HRESULT hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocators[m_frameIndex].Get(),
        nullptr,
        IID_PPV_ARGS(&m_commandList)
    );
    if (FAILED(hr)) {
        return false;
    }

    m_commandList->Close();
    return true;
}

bool RenderDevice::CreateFence() {
    HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr)) {
        return false;
    }

    m_fenceValues[m_frameIndex] = 1;

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) {
        return false;
    }

    return true;
}

bool RenderDevice::CreateSRVHeap() {
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = MaxSRVDescriptors;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap));
    if (FAILED(hr)) {
        return false;
    }

    m_srvHeap->SetName(L"Main SRV Heap");
    m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_nextSRVIndex = 0;

    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderDevice::AllocateSRV(uint32_t& outIndex) {
    if (m_nextSRVIndex >= MaxSRVDescriptors) {
        outIndex = UINT32_MAX;
        return D3D12_CPU_DESCRIPTOR_HANDLE{};
    }

    outIndex = m_nextSRVIndex++;

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(
        m_srvHeap->GetCPUDescriptorHandleForHeapStart(),
        outIndex,
        m_srvDescriptorSize
    );

    return cpuHandle;
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderDevice::GetSRVGPUHandle(uint32_t index) const {
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(
        m_srvHeap->GetGPUDescriptorHandleForHeapStart(),
        index,
        m_srvDescriptorSize
    );

    return gpuHandle;
}

void RenderDevice::MoveToNextFrame() {
    const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];

    HRESULT hr = m_commandQueue->Signal(m_fence.Get(), currentFenceValue);
    if (FAILED(hr)) {
        char msg[128];
        sprintf_s(msg, "MoveToNextFrame: Signal FAILED hr=0x%08X\n", hr);
        OutputDebugStringA(msg);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    UINT64 completedValue = m_fence->GetCompletedValue();

    if (completedValue < m_fenceValues[m_frameIndex]) {
        hr = m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
        if (FAILED(hr)) {
            char msg[128];
            sprintf_s(msg, "MoveToNextFrame: SetEventOnCompletion FAILED hr=0x%08X\n", hr);
            OutputDebugStringA(msg);
        }

        DWORD waitResult = WaitForSingleObject(m_fenceEvent, 5000);
        if (waitResult == WAIT_TIMEOUT) {
            OutputDebugStringA("MoveToNextFrame: WAIT TIMED OUT!\n");
        }
        else if (waitResult == WAIT_FAILED) {
            char msg[128];
            sprintf_s(msg, "MoveToNextFrame: WAIT FAILED error=%lu\n", GetLastError());
            OutputDebugStringA(msg);
        }
    }

    m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}