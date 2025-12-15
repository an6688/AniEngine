#include "Renderer.h"
#include "RenderDevice.h"
#include "Shader.h"
#include <d3dx12/d3dx12.h>

Renderer::Renderer()
    : m_device(nullptr)
    , m_triangleVertexBufferView({})
    , m_cubeVertexBufferView({})
    , m_cubeIndexBufferView({})
    , m_cubeIndexCount(0)
    , m_constantBufferDataBegin(nullptr)
    , m_cubeRotation(0.0f)
{
}

Renderer::~Renderer()
{
    Shutdown();
}

bool Renderer::Initialize(RenderDevice* device)
{
    m_device = device;

    if (!CreateTriangleGeometry())
        return false;

    if (!CreateBasicPipeline())
        return false;

    if (!CreateCubeGeometry())
        return false;

    if (!CreateCubePipeline())
        return false;

    return true;
}

void Renderer::Shutdown()
{
    // Unmap constant buffer if it was mapped
    if (m_constantBuffer && m_constantBufferDataBegin)
    {
        m_constantBuffer->Unmap(0, nullptr);
        m_constantBufferDataBegin = nullptr;
    }

    // Resources will be released automatically by ComPtr
    m_device = nullptr;
}

void Renderer::BeginFrame()
{
    // Currently empty - will be used for per-frame setup later
    // Example: updating constant buffers, clearing render targets, etc.
}

void Renderer::EndFrame()
{
    // Currently empty - will be used for per-frame cleanup later
}

void Renderer::DrawTriangle()
{
    ID3D12GraphicsCommandList* commandList = m_device->GetCommandList();

    // Set pipeline state
    commandList->SetPipelineState(m_basicPipelineState.Get());
    commandList->SetGraphicsRootSignature(m_basicRootSignature.Get());

    // Set primitive topology
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Set vertex buffer
    commandList->IASetVertexBuffers(0, 1, &m_triangleVertexBufferView);

    // Draw
    commandList->DrawInstanced(3, 1, 0, 0);
}

bool Renderer::CreateTriangleGeometry()
{
    // Vertex structure
    struct Vertex
    {
        float position[3];
        float color[4];
    };

    // Triangle vertices (same as before)
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

    HRESULT hr = m_device->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_triangleVertexBuffer)
    );

    if (FAILED(hr))
        return false;

    // Copy vertex data to buffer
    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);

    hr = m_triangleVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    if (FAILED(hr))
        return false;

    memcpy(pVertexDataBegin, triangleVertices, vertexBufferSize);
    m_triangleVertexBuffer->Unmap(0, nullptr);

    // Initialize vertex buffer view
    m_triangleVertexBufferView.BufferLocation = m_triangleVertexBuffer->GetGPUVirtualAddress();
    m_triangleVertexBufferView.StrideInBytes = sizeof(Vertex);
    m_triangleVertexBufferView.SizeInBytes = vertexBufferSize;

    return true;
}

bool Renderer::CreateBasicPipeline()
{
    // Create root signature
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

    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &error
    );

    if (FAILED(hr))
        return false;

    hr = m_device->GetDevice()->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&m_basicRootSignature)
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

    // Define input layout - now matches Mesh vertex format
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Create pipeline state object
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = m_basicRootSignature.Get();
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

    hr = m_device->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_basicPipelineState));

    return SUCCEEDED(hr);
}

bool Renderer::CreateCubeGeometry()
{
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 texCoord;
        DirectX::XMFLOAT4 color;
    };

    // Cube vertices - now with normals and texcoords
    Vertex cubeVertices[] =
    {
        // Front face (red) - normal pointing toward camera
        { { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        { { -0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        { {  0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },

        // Back face (green)
        { { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { -0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { {  0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },

        // Top face (blue)
        { { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
        { { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },

        // Bottom face (yellow)
        { { -0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } },
        { { -0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } },
        { {  0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } },
        { {  0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } },

        // Left face (cyan)
        { { -0.5f, -0.5f,  0.5f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 1.0f, 1.0f } },
        { { -0.5f,  0.5f,  0.5f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f } },
        { { -0.5f,  0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f } },
        { { -0.5f, -0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 1.0f, 1.0f } },

        // Right face (magenta)
        { {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 1.0f, 1.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f } },
        { {  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 1.0f, 1.0f } }
    };

    // Indices for cube (2 triangles per face, 6 faces)
    UINT16 cubeIndices[] =
    {
        // Front
        0, 1, 2,    0, 2, 3,
        // Back
        4, 6, 5,    4, 7, 6,
        // Top
        8, 9, 10,   8, 10, 11,
        // Bottom
        12, 14, 13, 12, 15, 14,
        // Left
        16, 17, 18, 16, 18, 19,
        // Right
        20, 22, 21, 20, 23, 22
    };

    m_cubeIndexCount = _countof(cubeIndices);

    const UINT vertexBufferSize = sizeof(cubeVertices);
    const UINT indexBufferSize = sizeof(cubeIndices);

    // Create vertex buffer
    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

    HRESULT hr = m_device->GetDevice()->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_cubeVertexBuffer)
    );

    if (FAILED(hr))
        return false;

    // Copy vertex data
    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);

    hr = m_cubeVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    if (FAILED(hr))
        return false;

    memcpy(pVertexDataBegin, cubeVertices, vertexBufferSize);
    m_cubeVertexBuffer->Unmap(0, nullptr);

    // Initialize vertex buffer view
    m_cubeVertexBufferView.BufferLocation = m_cubeVertexBuffer->GetGPUVirtualAddress();
    m_cubeVertexBufferView.StrideInBytes = sizeof(Vertex);
    m_cubeVertexBufferView.SizeInBytes = vertexBufferSize;

    // Create index buffer
    CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

    hr = m_device->GetDevice()->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &indexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_cubeIndexBuffer)
    );

    if (FAILED(hr))
        return false;

    // Copy index data
    UINT8* pIndexDataBegin;
    hr = m_cubeIndexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
    if (FAILED(hr))
        return false;

    memcpy(pIndexDataBegin, cubeIndices, indexBufferSize);
    m_cubeIndexBuffer->Unmap(0, nullptr);

    // Initialize index buffer view
    m_cubeIndexBufferView.BufferLocation = m_cubeIndexBuffer->GetGPUVirtualAddress();
    m_cubeIndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_cubeIndexBufferView.SizeInBytes = indexBufferSize;

    return true;
}

bool Renderer::CreateCubePipeline()
{
    // Create root signature with constant buffer
    CD3DX12_ROOT_PARAMETER rootParameter;
    rootParameter.InitAsConstantBufferView(0);  // b0 in shader

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
        1,
        &rootParameter,
        0,
        nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &error
    );

    if (FAILED(hr))
        return false;

    hr = m_device->GetDevice()->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&m_cubeRootSignature)
    );

    if (FAILED(hr))
        return false;

    // Compile cube shaders
    Shader vertexShader;
    Shader pixelShader;

    if (!vertexShader.CompileFromFile(L"Shaders/CubeVertexShader.hlsl", "main", "vs_5_1"))
        return false;

    if (!pixelShader.CompileFromFile(L"Shaders/CubePixelShader.hlsl", "main", "ps_5_1"))
        return false;

    // Define input layout - matches the new vertex format
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Create pipeline state
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = m_cubeRootSignature.Get();
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

    hr = m_device->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_cubePipelineState));

    if (FAILED(hr))
        return false;

    // Create constant buffer (256-byte aligned for DX12)
    const UINT constantBufferSize = (sizeof(DirectX::XMMATRIX) + 255) & ~255;

    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);

    hr = m_device->GetDevice()->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_constantBuffer)
    );

    if (FAILED(hr))
        return false;

    // Map constant buffer (keep it mapped for updates)
    CD3DX12_RANGE readRange(0, 0);
    hr = m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_constantBufferDataBegin));

    return SUCCEEDED(hr);
}

void Renderer::DrawCube(float deltaTime)
{
    using namespace DirectX;

    // Update rotation
    m_cubeRotation += deltaTime * 1.0f;  // 1 radian per second

    // Create transformation matrices
    XMMATRIX world = XMMatrixRotationY(m_cubeRotation) * XMMatrixRotationX(m_cubeRotation * 0.5f);

    // Camera position
    XMVECTOR eyePosition = XMVectorSet(0.0f, 0.0f, -3.0f, 0.0f);
    XMVECTOR focusPosition = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    XMVECTOR upDirection = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(eyePosition, focusPosition, upDirection);

    // Projection (perspective)
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, 1280.0f / 720.0f, 0.1f, 100.0f);

    // Combined matrix
    XMMATRIX worldViewProjection = world * view * projection;

    // Update constant buffer
    UpdateConstantBuffer(worldViewProjection);

    // Get command list
    ID3D12GraphicsCommandList* commandList = m_device->GetCommandList();

    // Set pipeline state
    commandList->SetPipelineState(m_cubePipelineState.Get());
    commandList->SetGraphicsRootSignature(m_cubeRootSignature.Get());

    // Set constant buffer
    commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress());

    // Set primitive topology
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Set vertex and index buffers
    commandList->IASetVertexBuffers(0, 1, &m_cubeVertexBufferView);
    commandList->IASetIndexBuffer(&m_cubeIndexBufferView);

    // Draw
    commandList->DrawIndexedInstanced(m_cubeIndexCount, 1, 0, 0, 0);
}

void Renderer::UpdateConstantBuffer(const DirectX::XMMATRIX& matrix)
{
    DirectX::XMMATRIX transposed = DirectX::XMMatrixTranspose(matrix);

    // Copy TRANSPOSED matrix to constant buffer
    memcpy(m_constantBufferDataBegin, &transposed, sizeof(transposed));
}