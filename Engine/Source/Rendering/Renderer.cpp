#include "Renderer.h"
#include "RenderDevice.h"
#include "Shader.h"
#include "Camera.h"
#include "Texture.h"
#include "TextureManager.h"
#include "Material.h"
#include "Mesh.h"
#include "VertexFormats.h"
#include "Core/ImGuiManager.h"
#include <d3dx12/d3dx12.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

Renderer::Renderer()
    : m_device(nullptr)
    , m_textureManager(nullptr)
    , m_renderSettings(nullptr)
    , m_cubeVertexBufferView({})
    , m_cubeIndexBufferView({})
    , m_cubeIndexCount(0)
    , m_constantBufferDataBegin(nullptr)
    , m_transformConstantBufferBegin(nullptr)
    , m_materialConstantBufferBegin(nullptr)
    , m_lightingConstantBufferBegin(nullptr)
    , m_currentTransformOffset(0)
    , m_currentMaterialOffset(0)
    , m_cubeRotation(0.0f) {
}

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize(RenderDevice* device, TextureManager* textureManager) {
    m_device = device;
    m_textureManager = textureManager;

    if (!CreateCubeGeometry()) {
        return false;
    }
    if (!CreateSimplePipeline()) {
        return false;
    }
    if (!CreatePBRPipeline()) {
        return false;
    }
    if (!CreateLightingConstantBuffer()) {
        return false;
    }

    return true;
}

void Renderer::Shutdown() {
    if (m_constantBuffer && m_constantBufferDataBegin) {
        m_constantBuffer->Unmap(0, nullptr);
        m_constantBufferDataBegin = nullptr;
    }

    if (m_transformConstantBuffer && m_transformConstantBufferBegin) {
        m_transformConstantBuffer->Unmap(0, nullptr);
        m_transformConstantBufferBegin = nullptr;
    }

    if (m_materialConstantBuffer && m_materialConstantBufferBegin) {
        m_materialConstantBuffer->Unmap(0, nullptr);
        m_materialConstantBufferBegin = nullptr;
    }

    if (m_lightingConstantBuffer && m_lightingConstantBufferBegin) {
        m_lightingConstantBuffer->Unmap(0, nullptr);
        m_lightingConstantBufferBegin = nullptr;
    }

    m_materialDescriptorCache.clear();
    m_device = nullptr;
    m_textureManager = nullptr;
    m_renderSettings = nullptr;
}

void Renderer::BeginFrame() {
    m_currentTransformOffset = 0;
    m_currentMaterialOffset = 0;

    UpdateLightingConstants();
}

void Renderer::EndFrame() {
}

bool Renderer::CreateLightingConstantBuffer() {
    const UINT bufferSize = CB_ALIGNMENT;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    HRESULT hr = m_device->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_lightingConstantBuffer)
    );

    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create lighting constant buffer\n");
        return false;
    }

    CD3DX12_RANGE readRange(0, 0);
    hr = m_lightingConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_lightingConstantBufferBegin));
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to map lighting constant buffer\n");
        return false;
    }

    return true;
}

void Renderer::UpdateLightingConstants() {
    if (!m_lightingConstantBufferBegin) {
        return;
    }

    LightingConstants lighting = {};

    if (m_renderSettings) {
        lighting.lightDirection = glm::normalize(glm::vec3(
            m_renderSettings->lightDirection[0],
            m_renderSettings->lightDirection[1],
            m_renderSettings->lightDirection[2]
        ));
        lighting.lightIntensity = m_renderSettings->lightIntensity;
        lighting.ambientIntensity = m_renderSettings->ambientIntensity;
    }
    else {
        lighting.lightDirection = glm::normalize(glm::vec3(0.5f, 1.0f, 0.5f));
        lighting.lightIntensity = 2.0f;
        lighting.ambientIntensity = 0.1f;
    }

    memcpy(m_lightingConstantBufferBegin, &lighting, sizeof(LightingConstants));
}

bool Renderer::CreateCubeGeometry() {
    Vertex cubeVertices[] = {
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

    uint16_t cubeIndices[] = {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        8, 9, 10, 8, 10, 11,
        12, 14, 13, 12, 15, 14,
        16, 17, 18, 16, 18, 19,
        20, 22, 21, 20, 23, 22
    };

    m_cubeIndexCount = _countof(cubeIndices);

    const UINT vertexBufferSize = sizeof(cubeVertices);
    const UINT indexBufferSize = sizeof(cubeIndices);

    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

    HRESULT hr = m_device->GetDevice()->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &vertexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_cubeVertexBuffer));
    if (FAILED(hr)) {
        return false;
    }

    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);
    hr = m_cubeVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    if (FAILED(hr)) {
        return false;
    }

    memcpy(pVertexDataBegin, cubeVertices, vertexBufferSize);
    m_cubeVertexBuffer->Unmap(0, nullptr);

    m_cubeVertexBufferView.BufferLocation = m_cubeVertexBuffer->GetGPUVirtualAddress();
    m_cubeVertexBufferView.StrideInBytes = sizeof(Vertex);
    m_cubeVertexBufferView.SizeInBytes = vertexBufferSize;

    CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
    hr = m_device->GetDevice()->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &indexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_cubeIndexBuffer));
    if (FAILED(hr)) {
        return false;
    }

    UINT8* pIndexDataBegin;
    hr = m_cubeIndexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
    if (FAILED(hr)) {
        return false;
    }

    memcpy(pIndexDataBegin, cubeIndices, indexBufferSize);
    m_cubeIndexBuffer->Unmap(0, nullptr);

    m_cubeIndexBufferView.BufferLocation = m_cubeIndexBuffer->GetGPUVirtualAddress();
    m_cubeIndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_cubeIndexBufferView.SizeInBytes = indexBufferSize;

    return true;
}

bool Renderer::CreateSimplePipeline() {
    CD3DX12_ROOT_PARAMETER rootParameter;
    rootParameter.InitAsConstantBufferView(0);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(1, &rootParameter, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        &signature, &error);
    if (FAILED(hr)) {
        return false;
    }

    hr = m_device->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(),
        signature->GetBufferSize(), IID_PPV_ARGS(&m_cubeRootSignature));
    if (FAILED(hr)) {
        return false;
    }

    Shader vertexShader;
    Shader pixelShader;

    if (!vertexShader.CompileFromFile(L"Shaders/TexturedVS.hlsl", "main", "vs_5_1")) {
        return false;
    }
    if (!pixelShader.CompileFromFile(L"Shaders/TexturedPS.hlsl", "main", "ps_5_1")) {
        return false;
    }

    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
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
    if (FAILED(hr)) {
        return false;
    }

    const UINT constantBufferSize = 256;
    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);

    hr = m_device->GetDevice()->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_constantBuffer));
    if (FAILED(hr)) {
        return false;
    }

    CD3DX12_RANGE readRange(0, 0);
    hr = m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_constantBufferDataBegin));
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

bool Renderer::CreatePBRPipeline() {
    // Root signature with 4 parameters:
    // 0: Transform CBV (b0)
    // 1: Material CBV (b1)
    // 2: Texture descriptor table (t0-t4)
    // 3: Lighting CBV (b2)

    CD3DX12_DESCRIPTOR_RANGE1 srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0);

    CD3DX12_ROOT_PARAMETER1 rootParams[4];
    rootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
    rootParams[1].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParams[2].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParams[3].InitAsConstantBufferView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
    rootSigDesc.Init_1_1(_countof(rootParams), rootParams, 1, &sampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
        &signature, &error);
    if (FAILED(hr)) {
        if (error) {
            OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        }
        return false;
    }

    hr = m_device->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(),
        signature->GetBufferSize(), IID_PPV_ARGS(&m_pbrRootSignature));
    if (FAILED(hr)) {
        return false;
    }

    Shader pbrVS;
    Shader pbrPS;

    if (!pbrVS.CompileFromFile(L"Shaders/PBRVS.hlsl", "main", "vs_5_1")) {
        OutputDebugStringA("Failed to compile PBRVS.hlsl\n");
        return false;
    }
    if (!pbrPS.CompileFromFile(L"Shaders/PBRPS.hlsl", "main", "ps_5_1")) {
        OutputDebugStringA("Failed to compile PBRPS.hlsl\n");
        return false;
    }

    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    psoDesc.pRootSignature = m_pbrRootSignature.Get();
    psoDesc.VS = pbrVS.GetBytecode();
    psoDesc.PS = pbrPS.GetBytecode();
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

    hr = m_device->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pbrPipelineState));
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create PBR pipeline state\n");
        return false;
    }

    // Create wireframe variant
    D3D12_GRAPHICS_PIPELINE_STATE_DESC wireframePsoDesc = psoDesc;
    wireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;

    hr = m_device->GetDevice()->CreateGraphicsPipelineState(&wireframePsoDesc, IID_PPV_ARGS(&m_pbrPipelineStateWireframe));
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create wireframe PSO\n");
        // Non-fatal
    }

    // Create transform constant buffer (ring buffer)
    const UINT transformBufferSize = MAX_INSTANCES_PER_FRAME * CB_ALIGNMENT;
    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(transformBufferSize);

    hr = m_device->GetDevice()->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_transformConstantBuffer));
    if (FAILED(hr)) {
        return false;
    }

    CD3DX12_RANGE readRange(0, 0);
    hr = m_transformConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_transformConstantBufferBegin));
    if (FAILED(hr)) {
        return false;
    }

    // Create material constant buffer (ring buffer)
    const UINT materialBufferSize = MAX_INSTANCES_PER_FRAME * CB_ALIGNMENT;
    bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize);

    hr = m_device->GetDevice()->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_materialConstantBuffer));
    if (FAILED(hr)) {
        return false;
    }

    hr = m_materialConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_materialConstantBufferBegin));
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

uint32_t Renderer::GetOrCreateMaterialDescriptorTable(Material* material) {
    if (!material || !m_textureManager) {
        return UINT32_MAX;
    }

    auto it = m_materialDescriptorCache.find(material);
    if (it != m_materialDescriptorCache.end()) {
        return it->second;
    }

    uint32_t baseIndex;
    D3D12_CPU_DESCRIPTOR_HANDLE baseHandle = m_device->AllocateSRV(baseIndex);
    if (baseIndex == UINT32_MAX) {
        return UINT32_MAX;
    }

    // Allocate 4 more for slots 1-4
    uint32_t dummy;
    m_device->AllocateSRV(dummy);
    m_device->AllocateSRV(dummy);
    m_device->AllocateSRV(dummy);
    m_device->AllocateSRV(dummy);

    auto defaultWhite = m_textureManager->GetDefaultWhiteTexture();
    auto defaultNormal = m_textureManager->GetDefaultNormalTexture();
    auto defaultBlack = m_textureManager->GetDefaultBlackTexture();

    ID3D12Device* device = m_device->GetDevice();
    uint32_t descriptorSize = m_device->GetSRVDescriptorSize();

    auto getHandle = [&](uint32_t offset) -> D3D12_CPU_DESCRIPTOR_HANDLE {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = baseHandle;
        handle.ptr += offset * descriptorSize;
        return handle;
        };

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    // Slot 0: Base Color (t0)
    {
        Texture* tex = material->baseColorTexture ? material->baseColorTexture.get() : defaultWhite.get();
        srvDesc.Format = tex->GetResource()->GetDesc().Format;
        device->CreateShaderResourceView(tex->GetResource(), &srvDesc, getHandle(0));
    }

    // Slot 1: Metallic-Roughness (t1)
    {
        Texture* tex = material->metallicRoughnessTexture ? material->metallicRoughnessTexture.get() : defaultWhite.get();
        srvDesc.Format = tex->GetResource()->GetDesc().Format;
        device->CreateShaderResourceView(tex->GetResource(), &srvDesc, getHandle(1));
    }

    // Slot 2: Normal (t2)
    {
        Texture* tex = material->normalTexture ? material->normalTexture.get() : defaultNormal.get();
        srvDesc.Format = tex->GetResource()->GetDesc().Format;
        device->CreateShaderResourceView(tex->GetResource(), &srvDesc, getHandle(2));
    }

    // Slot 3: Occlusion (t3)
    {
        Texture* tex = material->occlusionTexture ? material->occlusionTexture.get() : defaultWhite.get();
        srvDesc.Format = tex->GetResource()->GetDesc().Format;
        device->CreateShaderResourceView(tex->GetResource(), &srvDesc, getHandle(3));
    }

    // Slot 4: Emissive (t4)
    {
        Texture* tex = material->emissiveTexture ? material->emissiveTexture.get() : defaultBlack.get();
        srvDesc.Format = tex->GetResource()->GetDesc().Format;
        device->CreateShaderResourceView(tex->GetResource(), &srvDesc, getHandle(4));
    }

    m_materialDescriptorCache[material] = baseIndex;
    return baseIndex;
}

void Renderer::DrawCube(float deltaTime) {
    m_cubeRotation += deltaTime * 1.0f;

    glm::mat4 world = glm::rotate(glm::mat4(1.0f), m_cubeRotation, glm::vec3(0.0f, 1.0f, 0.0f));
    world = glm::rotate(world, m_cubeRotation * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));

    glm::vec3 eyePosition(0.0f, 0.0f, -3.0f);
    glm::vec3 focusPosition(0.0f, 0.0f, 0.0f);
    glm::vec3 upDirection(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(eyePosition, focusPosition, upDirection);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
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

void Renderer::DrawMeshTextured(Mesh* mesh, const glm::mat4& transform, Camera* camera) {
    if (!mesh || !mesh->IsValid() || !camera) {
        return;
    }

    if (m_currentTransformOffset >= MAX_INSTANCES_PER_FRAME) {
        OutputDebugStringA("Warning: Exceeded MAX_INSTANCES_PER_FRAME!\n");
        return;
    }

    ID3D12GraphicsCommandList* commandList = m_device->GetCommandList();

    // Select PSO based on wireframe mode
    if (m_renderSettings && m_renderSettings->wireframeMode && m_pbrPipelineStateWireframe) {
        commandList->SetPipelineState(m_pbrPipelineStateWireframe.Get());
    }
    else {
        commandList->SetPipelineState(m_pbrPipelineState.Get());
    }

    commandList->SetGraphicsRootSignature(m_pbrRootSignature.Get());

    // Set descriptor heap
    ID3D12DescriptorHeap* heaps[] = { m_device->GetSRVHeap() };
    commandList->SetDescriptorHeaps(1, heaps);

    // Calculate matrices
    glm::mat4 view = camera->GetViewMatrix();
    glm::mat4 projection = camera->GetProjectionMatrix();
    glm::mat4 worldViewProjection = projection * view * transform;
    glm::mat4 worldInverseTranspose = glm::transpose(glm::inverse(transform));

    // Update transform constants
    TransformConstants transformConsts;
    transformConsts.worldViewProjection = glm::transpose(worldViewProjection);
    transformConsts.world = glm::transpose(transform);
    transformConsts.worldInverseTranspose = glm::transpose(worldInverseTranspose);
    transformConsts.cameraPosition = camera->GetPosition();
    transformConsts.padding = 0.0f;

    UINT transformByteOffset = m_currentTransformOffset * CB_ALIGNMENT;
    memcpy(m_transformConstantBufferBegin + transformByteOffset, &transformConsts, sizeof(TransformConstants));

    // Get material
    auto material = mesh->GetMaterial();

    // Update material constants
    PBRMaterialConstants matConsts = {};

    if (material) {
        matConsts.baseColorFactor = material->baseColorFactor;
        matConsts.metallicFactor = material->metallicFactor;
        matConsts.roughnessFactor = material->roughnessFactor;
        matConsts.normalScale = material->normalScale;
        matConsts.occlusionStrength = material->occlusionStrength;
        matConsts.emissiveFactor = material->emissiveFactor;
        matConsts.alphaCutoff = material->alphaCutoff;

        matConsts.hasBaseColorTexture = material->baseColorTexture ? 1.0f : 0.0f;
        matConsts.hasMetallicRoughnessTexture = material->metallicRoughnessTexture ? 1.0f : 0.0f;
        matConsts.hasNormalTexture = material->normalTexture ? 1.0f : 0.0f;
        matConsts.hasOcclusionTexture = material->occlusionTexture ? 1.0f : 0.0f;
        matConsts.hasEmissiveTexture = material->emissiveTexture ? 1.0f : 0.0f;

        uint32_t descriptorTableIndex = GetOrCreateMaterialDescriptorTable(material.get());
        if (descriptorTableIndex != UINT32_MAX) {
            commandList->SetGraphicsRootDescriptorTable(2, m_device->GetSRVGPUHandle(descriptorTableIndex));
        }
    }
    else {
        matConsts.baseColorFactor = glm::vec4(1.0f);
        matConsts.metallicFactor = 0.0f;
        matConsts.roughnessFactor = 0.5f;
        matConsts.normalScale = 1.0f;
        matConsts.occlusionStrength = 1.0f;
        matConsts.emissiveFactor = glm::vec3(0.0f);
        matConsts.alphaCutoff = 0.5f;
        matConsts.hasBaseColorTexture = 0.0f;
        matConsts.hasMetallicRoughnessTexture = 0.0f;
        matConsts.hasNormalTexture = 0.0f;
        matConsts.hasOcclusionTexture = 0.0f;
        matConsts.hasEmissiveTexture = 0.0f;
    }

    UINT materialByteOffset = m_currentMaterialOffset * CB_ALIGNMENT;
    memcpy(m_materialConstantBufferBegin + materialByteOffset, &matConsts, sizeof(PBRMaterialConstants));

    // Bind constant buffers
    D3D12_GPU_VIRTUAL_ADDRESS transformCBAddress = m_transformConstantBuffer->GetGPUVirtualAddress() + transformByteOffset;
    D3D12_GPU_VIRTUAL_ADDRESS materialCBAddress = m_materialConstantBuffer->GetGPUVirtualAddress() + materialByteOffset;
    D3D12_GPU_VIRTUAL_ADDRESS lightingCBAddress = m_lightingConstantBuffer->GetGPUVirtualAddress();

    commandList->SetGraphicsRootConstantBufferView(0, transformCBAddress);
    commandList->SetGraphicsRootConstantBufferView(1, materialCBAddress);
    commandList->SetGraphicsRootConstantBufferView(3, lightingCBAddress);

    // Draw
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VERTEX_BUFFER_VIEW vbv = mesh->GetVertexBufferView();
    D3D12_INDEX_BUFFER_VIEW ibv = mesh->GetIndexBufferView();
    commandList->IASetVertexBuffers(0, 1, &vbv);
    commandList->IASetIndexBuffer(&ibv);

    commandList->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);

    m_currentTransformOffset++;
    m_currentMaterialOffset++;
}

void Renderer::UpdateConstantBuffer(const glm::mat4& matrix) {
    glm::mat4 m = glm::transpose(matrix);
    memcpy(m_constantBufferDataBegin, &m, sizeof(m));
}