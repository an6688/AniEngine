#include "Renderer.h"
#include "RenderDevice.h"
#include "Shader.h"
#include "Camera.h"
#include "Texture.h"
#include "Material.h"
#include <d3dx12/d3dx12.h>
#include <glm/gtc/type_ptr.hpp>
#include "VertexFormats.h"
#include "Mesh.h"

Renderer::Renderer()
    : m_device(nullptr)
    , m_cubeVertexBufferView({})
    , m_cubeIndexBufferView({})
    , m_cubeIndexCount(0)
    , m_constantBufferDataBegin(nullptr)
    , m_texturedConstantBufferBegin(nullptr)
    , m_materialConstantBufferBegin(nullptr)
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

    if (!CreateCubeGeometry())
        return false;

    if (!CreateCubePipeline())
        return false;

    // Create textured pipeline
    if (!CreateTexturedPipeline())
        return false;

    return true;
}

void Renderer::Shutdown()
{
    if (m_constantBuffer && m_constantBufferDataBegin)
    {
        m_constantBuffer->Unmap(0, nullptr);
        m_constantBufferDataBegin = nullptr;
    }

    if (m_texturedConstantBuffer && m_texturedConstantBufferBegin)
    {
        m_texturedConstantBuffer->Unmap(0, nullptr);
        m_texturedConstantBufferBegin = nullptr;
    }

    if (m_materialConstantBuffer && m_materialConstantBufferBegin)
    {
        m_materialConstantBuffer->Unmap(0, nullptr);
        m_materialConstantBufferBegin = nullptr;
    }

    m_device = nullptr;
}

void Renderer::BeginFrame()
{
}

void Renderer::EndFrame()
{
}

bool Renderer::CreateCubeGeometry()
{
    Vertex cubeVertices[] =
    {
        // Front face (red)
        { glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },
        { glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },
        { glm::vec3(0.5f,  0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },
        { glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },

        // Back face (green)
        { glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) },
        { glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) },
        { glm::vec3(0.5f,  0.5f,  0.5f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) },
        { glm::vec3(0.5f, -0.5f,  0.5f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) },

        // Top face (blue)
        { glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) },
        { glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) },
        { glm::vec3(0.5f,  0.5f,  0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) },
        { glm::vec3(0.5f,  0.5f, -0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) },

        // Bottom face (yellow)
        { glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) },
        { glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) },
        { glm::vec3(0.5f, -0.5f,  0.5f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) },
        { glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f) },

        // Right face (magenta)
        { glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) },
        { glm::vec3(0.5f,  0.5f, -0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) },
        { glm::vec3(0.5f,  0.5f,  0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) },
        { glm::vec3(0.5f, -0.5f,  0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) },

        // Left face (cyan)
        { glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f) },
        { glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f) },
        { glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f) },
        { glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f) },
    };

    uint16_t cubeIndices[] =
    {
        0, 1, 2, 0, 2, 3,       // Front
        4, 6, 5, 4, 7, 6,       // Back
        8, 9, 10, 8, 10, 11,    // Top
        12, 14, 13, 12, 15, 14, // Bottom
        16, 17, 18, 16, 18, 19, // Right
        20, 22, 21, 20, 23, 22  // Left
    };

    m_cubeIndexCount = _countof(cubeIndices);

    const UINT vertexBufferSize = sizeof(cubeVertices);
    const UINT indexBufferSize = sizeof(cubeIndices);

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

    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);

    hr = m_cubeVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    if (FAILED(hr))
        return false;

    memcpy(pVertexDataBegin, cubeVertices, vertexBufferSize);
    m_cubeVertexBuffer->Unmap(0, nullptr);

    m_cubeVertexBufferView.BufferLocation = m_cubeVertexBuffer->GetGPUVirtualAddress();
    m_cubeVertexBufferView.StrideInBytes = sizeof(Vertex);
    m_cubeVertexBufferView.SizeInBytes = vertexBufferSize;

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

    UINT8* pIndexDataBegin;
    hr = m_cubeIndexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
    if (FAILED(hr))
        return false;

    memcpy(pIndexDataBegin, cubeIndices, indexBufferSize);
    m_cubeIndexBuffer->Unmap(0, nullptr);

    m_cubeIndexBufferView.BufferLocation = m_cubeIndexBuffer->GetGPUVirtualAddress();
    m_cubeIndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_cubeIndexBufferView.SizeInBytes = indexBufferSize;

    return true;
}

bool Renderer::CreateCubePipeline()
{
    CD3DX12_ROOT_PARAMETER rootParameter;
    rootParameter.InitAsConstantBufferView(0);

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

    Shader vertexShader;
    Shader pixelShader;

    if (!vertexShader.CompileFromFile(L"Shaders/CubeVertexShader.hlsl", "main", "vs_5_1"))
        return false;

    if (!pixelShader.CompileFromFile(L"Shaders/CubePixelShader.hlsl", "main", "ps_5_1"))
        return false;

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    UINT layoutCount;
    D3D12_INPUT_ELEMENT_DESC* inputLayout = Vertex::GetInputLayout(layoutCount);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputLayout, layoutCount };
    psoDesc.pRootSignature = m_cubeRootSignature.Get();
    psoDesc.VS = vertexShader.GetBytecode();
    psoDesc.PS = pixelShader.GetBytecode();
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
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

    const UINT constantBufferSize = (sizeof(glm::mat4) + 255) & ~255;

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

    CD3DX12_RANGE readRange(0, 0);
    hr = m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_constantBufferDataBegin));

    return SUCCEEDED(hr);
}

bool Renderer::CreateTexturedPipeline()
{
    // Root signature with:
    // [0] CBV - Transform constants (b0)
    // [1] CBV - Material constants (b1)
    // [2] Descriptor table - Texture SRV (t0)
    // Static sampler (s0)

    CD3DX12_ROOT_PARAMETER rootParams[3];

    // Transform constants at b0
    rootParams[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    // Material constants at b1
    rootParams[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    // Texture SRV table at t0
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);  // 1 texture at t0
    rootParams[2].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

    // Static sampler
    CD3DX12_STATIC_SAMPLER_DESC sampler(
        0,                                  // Shader register s0
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,   // Linear filtering
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,   // U wrap
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,   // V wrap
        D3D12_TEXTURE_ADDRESS_MODE_WRAP    // W wrap
    );

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
    rootSigDesc.Init(
        _countof(rootParams),
        rootParams,
        1,
        &sampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootSigDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &error
    );

    if (FAILED(hr))
    {
        if (error)
        {
            OutputDebugStringA((char*)error->GetBufferPointer());
        }
        return false;
    }

    hr = m_device->GetDevice()->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&m_texturedRootSignature)
    );

    if (FAILED(hr))
        return false;

    // Compile shaders
    Shader vertexShader;
    Shader pixelShader;

    if (!vertexShader.CompileFromFile(L"Shaders/TexturedVS.hlsl", "main", "vs_5_1"))
    {
        OutputDebugStringA("Failed to compile TexturedVS.hlsl\n");
        return false;
    }

    if (!pixelShader.CompileFromFile(L"Shaders/TexturedPS.hlsl", "main", "ps_5_1"))
    {
        OutputDebugStringA("Failed to compile TexturedPS.hlsl\n");
        return false;
    }

    // Input layout
    UINT layoutCount;
    D3D12_INPUT_ELEMENT_DESC* inputLayout = Vertex::GetInputLayout(layoutCount);

    // Pipeline state
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputLayout, layoutCount };
    psoDesc.pRootSignature = m_texturedRootSignature.Get();
    psoDesc.VS = vertexShader.GetBytecode();
    psoDesc.PS = pixelShader.GetBytecode();
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    hr = m_device->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_texturedPipelineState));

    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create textured pipeline state\n");
        return false;
    }

    // Create constant buffers
    const UINT transformBufferSize = (sizeof(TexturedConstants) + 255) & ~255;
    const UINT materialBufferSize = (sizeof(MaterialConstants) + 255) & ~255;

    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);

    // Transform constant buffer
    CD3DX12_RESOURCE_DESC transformBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(transformBufferSize);
    hr = m_device->GetDevice()->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &transformBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_texturedConstantBuffer)
    );

    if (FAILED(hr))
        return false;

    CD3DX12_RANGE readRange(0, 0);
    hr = m_texturedConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_texturedConstantBufferBegin));
    if (FAILED(hr))
        return false;

    // Material constant buffer
    CD3DX12_RESOURCE_DESC materialBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize);
    hr = m_device->GetDevice()->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &materialBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_materialConstantBuffer)
    );

    if (FAILED(hr))
        return false;

    hr = m_materialConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_materialConstantBufferBegin));
    if (FAILED(hr))
        return false;

    OutputDebugStringA("Textured pipeline created successfully\n");
    return true;
}

void Renderer::DrawCube(float deltaTime)
{
    m_cubeRotation += deltaTime * 1.0f;

    glm::mat4 world = glm::rotate(glm::mat4(1.0f), m_cubeRotation, glm::vec3(0.0f, 1.0f, 0.0f));
    world = glm::rotate(world, m_cubeRotation * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));

    glm::vec3 eyePosition(0.0f, 0.0f, -3.0f);
    glm::vec3 focusPosition(0.0f, 0.0f, 0.0f);
    glm::vec3 upDirection(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(eyePosition, focusPosition, upDirection);

    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        1280.0f / 720.0f,
        0.1f,
        100.0f
    );

    glm::mat4 worldViewProjection = projection * view * world;

    UpdateConstantBuffer(worldViewProjection);

    ID3D12GraphicsCommandList* commandList = m_device->GetCommandList();

    commandList->SetPipelineState(m_cubePipelineState.Get());
    commandList->SetGraphicsRootSignature(m_cubeRootSignature.Get());
    commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_cubeVertexBufferView);
    commandList->IASetIndexBuffer(&m_cubeIndexBufferView);
    commandList->DrawIndexedInstanced(m_cubeIndexCount, 1, 0, 0, 0);
}

void Renderer::DrawMeshTextured(Mesh* mesh, const glm::mat4& transform, Camera* camera)
{
    if (!mesh || !mesh->IsValid() || !camera)
        return;

    ID3D12GraphicsCommandList* commandList = m_device->GetCommandList();

    // Set pipeline and root signature
    commandList->SetPipelineState(m_texturedPipelineState.Get());
    commandList->SetGraphicsRootSignature(m_texturedRootSignature.Get());

    // Set descriptor heap for textures
    ID3D12DescriptorHeap* heaps[] = { m_device->GetSRVHeap() };
    commandList->SetDescriptorHeaps(1, heaps);

    // Update transform constants
    glm::mat4 view = camera->GetViewMatrix();
    glm::mat4 projection = camera->GetProjectionMatrix();

    TexturedConstants transformConsts;
    transformConsts.worldViewProjection = glm::transpose(projection * view * transform);
    transformConsts.world = glm::transpose(transform);
    transformConsts.cameraPosition = camera->GetPosition();
    transformConsts.padding = 0.0f;

    memcpy(m_texturedConstantBufferBegin, &transformConsts, sizeof(TexturedConstants));

    // Update material constants
    MaterialConstants matConsts;
    auto material = mesh->GetMaterial();

    if (material && material->baseColorTexture)
    {
        matConsts.baseColorFactor = material->baseColorFactor;
        matConsts.hasBaseColorTexture = 1.0f;

        // Bind texture
        commandList->SetGraphicsRootDescriptorTable(
            2,  // Root param index for texture
            m_device->GetSRVGPUHandle(material->baseColorTexture->GetSRVIndex())
        );
    }
    else
    {
        matConsts.baseColorFactor = glm::vec4(1.0f);  // White default
        matConsts.hasBaseColorTexture = 0.0f;

        // Still need to bind something - use a default texture if available
        // For now, we'll just not bind anything (shader handles hasBaseColorTexture = 0)
    }
    matConsts.padding = glm::vec3(0.0f);

    memcpy(m_materialConstantBufferBegin, &matConsts, sizeof(MaterialConstants));

    // Set constant buffers
    commandList->SetGraphicsRootConstantBufferView(0, m_texturedConstantBuffer->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, m_materialConstantBuffer->GetGPUVirtualAddress());

    // Set mesh buffers and draw
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    auto vbv = mesh->GetVertexBufferView();
    auto ibv = mesh->GetIndexBufferView();
    commandList->IASetVertexBuffers(0, 1, &vbv);
    commandList->IASetIndexBuffer(&ibv);

    commandList->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);
}

void Renderer::UpdateConstantBuffer(const glm::mat4& matrix)
{
    glm::mat4 m = glm::transpose(matrix);
    memcpy(m_constantBufferDataBegin, &m, sizeof(m));
}
