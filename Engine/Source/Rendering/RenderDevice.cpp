#include "RenderDevice.h"
#include "Shader.h"
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
    , m_vertexBufferView({})
{
    for (UINT i = 0; i < FrameBufferCount; ++i)
    {
        m_fenceValues[i] = 0;
    }
}

RenderDevice::~RenderDevice()
{
    Shutdown();
}

bool RenderDevice::Initialize(HWND hwnd, int width, int height)
{
    m_width = width;
    m_height = height;

    EnableDebugLayer();

    if (!CreateDevice()) return false;
    if (!CreateCommandQueue()) return false;
    if (!CreateSwapChain(hwnd, width, height)) return false;
    if (!CreateRenderTargets()) return false;
    if (!CreateDepthStencil(width, height)) return false;
    if (!CreateCommandAllocatorsAndList()) return false;
    if (!CreateFence()) return false;

    // Setup viewport
    m_viewport.TopLeftX = 0.0f;
    m_viewport.TopLeftY = 0.0f;
    m_viewport.Width = static_cast<float>(width);
    m_viewport.Height = static_cast<float>(height);
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    // Setup scissor rect
    m_scissorRect.left = 0;
    m_scissorRect.top = 0;
    m_scissorRect.right = width;
    m_scissorRect.bottom = height;

    // Create triangle resources
    if (!CreateTriangleResources())
        return false;

    return true;
}

void RenderDevice::Shutdown()
{
    // Wait for GPU to finish
    WaitForGPU();

    // Close fence event
    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    // COM objects will auto-release via ComPtr
}

void RenderDevice::BeginFrame()
{
    // Reset command allocator for this frame
    m_commandAllocators[m_frameIndex]->Reset();

    // Reset command list
    m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr);

    // Transition render target from PRESENT to RENDER_TARGET state
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    m_commandList->ResourceBarrier(1, &barrier);

    // Get render target view handle
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        m_frameIndex,
        m_rtvDescriptorSize
    );

    // Get depth stencil view handle
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(
        m_dsvHeap->GetCPUDescriptorHandleForHeapStart()
    );

    // Set render targets
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Clear render target
    const float clearColor[] = { 0.2f, 0.1f, 0.3f, 1.0f };  // Dark purple
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // Clear depth stencil
    m_commandList->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr
    );

    // Set viewport and scissor rect
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Render our triangle
    RenderTriangle();
}

void RenderDevice::EndFrame()
{
    // Transition render target from RENDER_TARGET to PRESENT state
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    m_commandList->ResourceBarrier(1, &barrier);

    // Close command list
    m_commandList->Close();

    // Execute command list
    ID3D12CommandList* commandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, commandLists);

    // Present the frame
    m_swapChain->Present(1, 0);  // 1 = vsync on, 0 = vsync off

    // Move to next frame
    MoveToNextFrame();
}

void RenderDevice::WaitForGPU()
{
    if (m_commandQueue && m_fence && m_fenceEvent)
    {
        // Signal and increment fence value
        const UINT64 fenceValue = m_fenceValues[m_frameIndex];
        m_commandQueue->Signal(m_fence.Get(), fenceValue);
        m_fenceValues[m_frameIndex]++;

        // Wait until fence is reached
        if (m_fence->GetCompletedValue() < fenceValue)
        {
            m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }
}

void RenderDevice::OnResize(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    // Wait for GPU
    WaitForGPU();

    // Release old resources
    for (UINT i = 0; i < FrameBufferCount; ++i)
    {
        m_renderTargets[i].Reset();
    }
    m_depthStencil.Reset();

    // Resize swap chain
    DXGI_SWAP_CHAIN_DESC desc = {};
    m_swapChain->GetDesc(&desc);
    m_swapChain->ResizeBuffers(
        FrameBufferCount,
        width,
        height,
        desc.BufferDesc.Format,
        desc.Flags
    );

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Recreate render targets and depth buffer
    CreateRenderTargets();
    CreateDepthStencil(width, height);

    // Update viewport
    m_viewport.Width = static_cast<float>(width);
    m_viewport.Height = static_cast<float>(height);

    m_scissorRect.right = width;
    m_scissorRect.bottom = height;

    m_width = width;
    m_height = height;
}

void RenderDevice::EnableDebugLayer()
{
#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
#endif
}

bool RenderDevice::CreateDevice()
{
    // Create DXGI factory
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return false;

    // Try to create hardware device
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;

    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Skip software adapter
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        // Try to create device
        hr = D3D12CreateDevice(
            adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        );

        if (SUCCEEDED(hr))
            break;
    }

    if (!m_device)
    {
        // Fallback to WARP device
        factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
        hr = D3D12CreateDevice(
            adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        );
    }

    return SUCCEEDED(hr);
}

bool RenderDevice::CreateCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    HRESULT hr = m_device->CreateCommandQueue(
        &queueDesc,
        IID_PPV_ARGS(&m_commandQueue)
    );

    return SUCCEEDED(hr);
}

bool RenderDevice::CreateSwapChain(HWND hwnd, int width, int height)
{
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return false;

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

    if (FAILED(hr)) return false;

    hr = swapChain1.As(&m_swapChain);
    if (FAILED(hr)) return false;

    // Disable Alt+Enter fullscreen toggle
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    return true;
}

bool RenderDevice::CreateRenderTargets()
{
    // Create RTV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = m_device->CreateDescriptorHeap(
        &rtvHeapDesc,
        IID_PPV_ARGS(&m_rtvHeap)
    );
    if (FAILED(hr)) return false;

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV
    );

    // Create render target views
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart()
    );

    for (UINT i = 0; i < FrameBufferCount; ++i)
    {
        hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
        if (FAILED(hr)) return false;

        m_device->CreateRenderTargetView(
            m_renderTargets[i].Get(),
            nullptr,
            rtvHandle
        );

        rtvHandle.Offset(1, m_rtvDescriptorSize);
    }

    return true;
}

bool RenderDevice::CreateDepthStencil(int width, int height)
{
    // Create DSV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = m_device->CreateDescriptorHeap(
        &dsvHeapDesc,
        IID_PPV_ARGS(&m_dsvHeap)
    );
    if (FAILED(hr)) return false;

    // Create depth stencil texture
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

    hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&m_depthStencil)
    );
    if (FAILED(hr)) return false;

    // Create depth stencil view
    m_device->CreateDepthStencilView(
        m_depthStencil.Get(),
        nullptr,
        m_dsvHeap->GetCPUDescriptorHandleForHeapStart()
    );

    return true;
}

bool RenderDevice::CreateCommandAllocatorsAndList()
{
    // Create command allocators (one per frame)
    for (UINT i = 0; i < FrameBufferCount; ++i)
    {
        HRESULT hr = m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_commandAllocators[i])
        );
        if (FAILED(hr)) return false;
    }

    // Create command list
    HRESULT hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocators[m_frameIndex].Get(),
        nullptr,
        IID_PPV_ARGS(&m_commandList)
    );
    if (FAILED(hr)) return false;

    // Command lists are created in recording state, close it
    m_commandList->Close();

    return true;
}

bool RenderDevice::CreateFence()
{
    HRESULT hr = m_device->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&m_fence)
    );
    if (FAILED(hr)) return false;

    m_fenceValues[m_frameIndex] = 1;

    // Create event for fence
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent)
        return false;

    return true;
}

void RenderDevice::MoveToNextFrame()
{
    // Signal fence with current value
    const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
    m_commandQueue->Signal(m_fence.Get(), currentFenceValue);

    // Move to next frame
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Wait if next frame is not ready yet
    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
    {
        m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Set fence value for next frame
    m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

bool RenderDevice::CreateTriangleResources()
{
    // Vertex data for a colored triangle
    struct Vertex
    {
        float position[3];
        float color[4];
    };

    // Simple triangle for now
    Vertex triangleVertices[] =
    {
        { {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },  // Top (red)
        { {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },  // Right (green)
        { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }   // Left (blue)
    };

    const UINT vertexBufferSize = sizeof(triangleVertices);

    // Create vertex buffer
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)
    );

    if (FAILED(hr))
        return false;

    // Copy vertex data to buffer
    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);  // Don't read from this resource on CPU

    hr = m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    if (FAILED(hr))
        return false;

    memcpy(pVertexDataBegin, triangleVertices, vertexBufferSize);
    m_vertexBuffer->Unmap(0, nullptr);

    // Initialize vertex buffer view
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = vertexBufferSize;

    // Create root signature, empty for this simple shader
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
        0,
        nullptr,
        0,
        nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &error
    );

    if (FAILED(hr))
        return false;

    hr = m_device->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)
    );

    if (FAILED(hr))
        return false;

    // Compile shaders
    Shader vertexShader;
    Shader pixelShader;

    if (!vertexShader.CompileFromFile(L"Shaders/VertexShader.hlsl", "main", "vs_5_1"))
        return false;

    if (!pixelShader.CompileFromFile(L"Shaders/PixelShader.hlsl", "main", "ps_5_1"))
        return false;

    // Define input layout
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Create pipeline state object
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = vertexShader.GetBytecode();
    psoDesc.PS = pixelShader.GetBytecode();
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));

    return SUCCEEDED(hr);
}

void RenderDevice::RenderTriangle()
{
    // Set pipeline state
    m_commandList->SetPipelineState(m_pipelineState.Get());
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    // Set primitive topology
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Set vertex buffer
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

    // Draw triangle
    m_commandList->DrawInstanced(3, 1, 0, 0);
}