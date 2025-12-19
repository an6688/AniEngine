#include "Renderer.h"
#include "RenderDevice.h"
#include "Shader.h"
#include "Camera.h"
#include "Texture.h"
#include "TextureManager.h"
#include "Material.h"
#include <d3dx12/d3dx12.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include "VertexFormats.h"
#include "Mesh.h"

Renderer::Renderer()
    : m_device(nullptr)
    , m_textureManager(nullptr)
    , m_cubeVertexBufferView({})
    , m_cubeIndexBufferView({})
    , m_cubeIndexCount(0)
    , m_constantBufferDataBegin(nullptr)
    , m_transformConstantBufferBegin(nullptr)
    , m_materialConstantBufferBegin(nullptr)
    , m_currentTransformOffset(0)
    , m_currentMaterialOffset(0)
    , m_cubeRotation(0.0f)
{
}

Renderer::~Renderer()
{
    Shutdown();
}

bool Renderer::Initialize(RenderDevice* device, TextureManager* textureManager)
{
    m_device = device;
    m_textureManager = textureManager;

    if (!CreateCubeGeometry())
        return false;

    if (!CreateSimplePipeline())
        return false;

    if (!CreatePBRPipeline())
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

    if (m_transformConstantBuffer && m_transformConstantBufferBegin)
    {
        m_transformConstantBuffer->Unmap(0, nullptr);
        m_transformConstantBufferBegin = nullptr;
    }

    if (m_materialConstantBuffer && m_materialConstantBufferBegin)
    {
        m_materialConstantBuffer->Unmap(0, nullptr);
        m_materialConstantBufferBegin = nullptr;
    }

    m_materialDescriptorCache.clear();
    m_device = nullptr;
    m_textureManager = nullptr;
}

void Renderer::BeginFrame()
{
    // Reset ring buffer offsets at the start of each frame
    m_currentTransformOffset = 0;
    m_currentMaterialOffset = 0;
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
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &vertexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_cubeVertexBuffer));
    if (FAILED(hr)) return false;

    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);
    hr = m_cubeVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    if (FAILED(hr)) return false;

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
    if (FAILED(hr)) return false;

    UINT8* pIndexDataBegin;
    hr = m_cubeIndexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
    if (FAILED(hr)) return false;

    memcpy(pIndexDataBegin, cubeIndices, indexBufferSize);
    m_cubeIndexBuffer->Unmap(0, nullptr);

    m_cubeIndexBufferView.BufferLocation = m_cubeIndexBuffer->GetGPUVirtualAddress();
    m_cubeIndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_cubeIndexBufferView.SizeInBytes = indexBufferSize;

    return true;
}

bool Renderer::CreateSimplePipeline()
{
    CD3DX12_ROOT_PARAMETER rootParameter;
    rootParameter.InitAsConstantBufferView(0);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
        1, &rootParameter,
        0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature, &error);

    if (FAILED(hr))
    {
        if (error) OutputDebugStringA((char*)error->GetBufferPointer());
        return false;
    }

    hr = m_device->GetDevice()->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&m_cubeRootSignature));
    if (FAILED(hr)) return false;

    Shader vertexShader;
    Shader pixelShader;

    if (!vertexShader.CompileFromFile(L"Shaders/TexturedVS.hlsl", "main", "vs_5_1")) {
        return false;
    }
    if (!pixelShader.CompileFromFile(L"Shaders/TexturedPS.hlsl", "main", "ps_5_1")) {
        return false;
    }

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
    if (FAILED(hr)) return false;

    // Constant buffer
    const UINT constantBufferSize = (sizeof(glm::mat4) + 255) & ~255;
    CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    hr = m_device->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &constantBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_constantBuffer));
    if (FAILED(hr)) return false;

    CD3DX12_RANGE readRange(0, 0);
    hr = m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_constantBufferDataBegin));
    if (FAILED(hr)) return false;

    return true;
}

bool Renderer::CreatePBRPipeline()
{
    // Root signature layout:
    // [0] CBV - Transform constants (b0) - ALL shaders (VS needs matrices, PS needs camera pos)
    // [1] CBV - Material constants (b1) - pixel shader
    // [2] Descriptor table - 5 textures (t0-t4) - pixel shader
    // Static sampler at s0

    CD3DX12_ROOT_PARAMETER rootParams[3];

    // Transform constants - visible to ALL shaders (both VS and PS need it)
    rootParams[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

    // Material constants - pixel shader only
    rootParams[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    // Texture descriptor table: 5 SRVs (baseColor, metallicRoughness, normal, occlusion, emissive)
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0);  // 5 textures starting at t0
    rootParams[2].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

    // Static sampler for texture filtering
    CD3DX12_STATIC_SAMPLER_DESC sampler(
        0,                                  // Register s0
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP
    );

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
    rootSigDesc.Init(_countof(rootParams), rootParams, 1, &sampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc,
        D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

    if (FAILED(hr))
    {
        if (error) OutputDebugStringA((char*)error->GetBufferPointer());
        return false;
    }

    hr = m_device->GetDevice()->CreateRootSignature(0,
        signature->GetBufferPointer(), signature->GetBufferSize(),
        IID_PPV_ARGS(&m_pbrRootSignature));
    if (FAILED(hr)) return false;

    // Compile shaders
    Shader vertexShader;
    Shader pixelShader;

    if (!vertexShader.CompileFromFile(L"Shaders/PBRVS.hlsl", "main", "vs_5_1"))
    {
        OutputDebugStringA("Failed to compile PBRVS.hlsl\n");
        return false;
    }

    if (!pixelShader.CompileFromFile(L"Shaders/PBRPS.hlsl", "main", "ps_5_1"))
    {
        OutputDebugStringA("Failed to compile PBRPS.hlsl\n");
        return false;
    }

    // Input layout
    UINT layoutCount;
    D3D12_INPUT_ELEMENT_DESC* inputLayout = Vertex::GetInputLayout(layoutCount);

    // Pipeline state
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputLayout, layoutCount };
    psoDesc.pRootSignature = m_pbrRootSignature.Get();
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

    hr = m_device->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pbrPipelineState));
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create PBR pipeline state\n");
        return false;
    }

    // Create LARGE constant buffers for ring-buffer style per-instance data
    // Each instance needs its own slot in the buffer, aligned to 256 bytes
    const UINT transformBufferSize = TRANSFORM_CB_ALIGNMENT * MAX_INSTANCES_PER_FRAME;
    const UINT materialBufferSize = MATERIAL_CB_ALIGNMENT * MAX_INSTANCES_PER_FRAME;

    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);

    // Transform constant buffer (large enough for all instances)
    CD3DX12_RESOURCE_DESC transformBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(transformBufferSize);
    hr = m_device->GetDevice()->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &transformBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_transformConstantBuffer));
    if (FAILED(hr)) return false;

    CD3DX12_RANGE readRange(0, 0);
    hr = m_transformConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_transformConstantBufferBegin));
    if (FAILED(hr)) return false;

    // Material constant buffer (large enough for all instances)
    CD3DX12_RESOURCE_DESC materialBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize);
    hr = m_device->GetDevice()->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &materialBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_materialConstantBuffer));
    if (FAILED(hr)) return false;

    hr = m_materialConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_materialConstantBufferBegin));
    if (FAILED(hr)) return false;

    OutputDebugStringA("PBR pipeline created successfully\n");
    return true;
}

uint32_t Renderer::GetOrCreateMaterialDescriptorTable(Material* material)
{
    if (!material)
        return UINT32_MAX;

    // Check cache first
    auto it = m_materialDescriptorCache.find(material);
    if (it != m_materialDescriptorCache.end())
    {
        return it->second;
    }

    // Allocate 5 contiguous descriptors for this material
    uint32_t baseIndex;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_device->AllocateSRV(baseIndex);

    if (cpuHandle.ptr == 0)
    {
        OutputDebugStringA("Failed to allocate SRV for material\n");
        return UINT32_MAX;
    }

    // We need 4 more descriptors (total 5)
    uint32_t dummy;
    for (int i = 0; i < 4; i++)
    {
        m_device->AllocateSRV(dummy);
    }

    // Get default textures for fallback
    auto defaultWhite = m_textureManager->GetDefaultWhiteTexture();
    auto defaultNormal = m_textureManager->GetDefaultNormalTexture();
    auto defaultBlack = m_textureManager->GetDefaultBlackTexture();

    // Helper to get CPU handle at offset
    auto getHandle = [&](uint32_t offset) -> D3D12_CPU_DESCRIPTOR_HANDLE {
        D3D12_CPU_DESCRIPTOR_HANDLE handle;
        handle.ptr = cpuHandle.ptr + offset * m_device->GetSRVDescriptorSize();
        return handle;
        };

    // Create SRVs for each texture slot
    auto device = m_device->GetDevice();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

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

    // Cache and return
    m_materialDescriptorCache[material] = baseIndex;

    char msg[128];
    sprintf_s(msg, "Created material descriptor table at index %u\n", baseIndex);
    OutputDebugStringA(msg);

    return baseIndex;
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

void Renderer::DrawMeshTextured(Mesh* mesh, const glm::mat4& transform, Camera* camera)
{
    if (!mesh || !mesh->IsValid() || !camera)
        return;

    // Check if we've exceeded our per frame instance limit
    if (m_currentTransformOffset >= MAX_INSTANCES_PER_FRAME)
    {
        OutputDebugStringA("Warning: Exceeded MAX_INSTANCES_PER_FRAME!\n");
        return;
    }

    ID3D12GraphicsCommandList* commandList = m_device->GetCommandList();

    // Set pipeline and root signature
    commandList->SetPipelineState(m_pbrPipelineState.Get());
    commandList->SetGraphicsRootSignature(m_pbrRootSignature.Get());

    // Set descriptor heap for textures
    ID3D12DescriptorHeap* heaps[] = { m_device->GetSRVHeap() };
    commandList->SetDescriptorHeaps(1, heaps);

    // Calculate matrices
    glm::mat4 view = camera->GetViewMatrix();
    glm::mat4 projection = camera->GetProjectionMatrix();
    glm::mat4 worldViewProjection = projection * view * transform;
    glm::mat4 worldInverseTranspose = glm::transpose(glm::inverse(transform));

    // Update transform constants at CURRENT OFFSET in the ring buffer
    TransformConstants transformConsts;
    transformConsts.worldViewProjection = glm::transpose(worldViewProjection);
    transformConsts.world = glm::transpose(transform);
    transformConsts.worldInverseTranspose = glm::transpose(worldInverseTranspose);
    transformConsts.cameraPosition = camera->GetPosition();
    transformConsts.padding = 0.0f;

    // Write to the current slot in the ring buffer
    UINT transformByteOffset = m_currentTransformOffset * TRANSFORM_CB_ALIGNMENT;
    memcpy(m_transformConstantBufferBegin + transformByteOffset, &transformConsts, sizeof(TransformConstants));

    // Get material from mesh
    auto material = mesh->GetMaterial();

    // Update material constants at CURRENT OFFSET
    PBRMaterialConstants matConsts = {};

    if (material)
    {
        matConsts.baseColorFactor = material->baseColorFactor;
        matConsts.metallicFactor = material->metallicFactor;
        matConsts.roughnessFactor = material->roughnessFactor;
        matConsts.normalScale = material->normalScale;
        matConsts.occlusionStrength = material->occlusionStrength;
        matConsts.emissiveFactor = material->emissiveFactor;
        matConsts.alphaCutoff = material->alphaCutoff;

        // Set texture flags
        matConsts.hasBaseColorTexture = material->baseColorTexture ? 1.0f : 0.0f;
        matConsts.hasMetallicRoughnessTexture = material->metallicRoughnessTexture ? 1.0f : 0.0f;
        matConsts.hasNormalTexture = material->normalTexture ? 1.0f : 0.0f;
        matConsts.hasOcclusionTexture = material->occlusionTexture ? 1.0f : 0.0f;
        matConsts.hasEmissiveTexture = material->emissiveTexture ? 1.0f : 0.0f;

        // Get or create contiguous descriptor table for this material
        uint32_t descriptorTableIndex = GetOrCreateMaterialDescriptorTable(material.get());

        if (descriptorTableIndex != UINT32_MAX)
        {
            commandList->SetGraphicsRootDescriptorTable(2,
                m_device->GetSRVGPUHandle(descriptorTableIndex));
        }
    }
    else
    {
        // No material - use defaults
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

    // Write material to the current slot in the ring buffer
    UINT materialByteOffset = m_currentMaterialOffset * MATERIAL_CB_ALIGNMENT;
    memcpy(m_materialConstantBufferBegin + materialByteOffset, &matConsts, sizeof(PBRMaterialConstants));

    // Set constant buffers with OFFSETS into the ring buffer
    D3D12_GPU_VIRTUAL_ADDRESS transformCBAddress = m_transformConstantBuffer->GetGPUVirtualAddress() + transformByteOffset;
    D3D12_GPU_VIRTUAL_ADDRESS materialCBAddress = m_materialConstantBuffer->GetGPUVirtualAddress() + materialByteOffset;

    commandList->SetGraphicsRootConstantBufferView(0, transformCBAddress);
    commandList->SetGraphicsRootConstantBufferView(1, materialCBAddress);

    // Set mesh buffers and draw
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    auto vbv = mesh->GetVertexBufferView();
    auto ibv = mesh->GetIndexBufferView();
    commandList->IASetVertexBuffers(0, 1, &vbv);
    commandList->IASetIndexBuffer(&ibv);

    commandList->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);

    // Advance to next slot for the next draw call
    m_currentTransformOffset++;
    m_currentMaterialOffset++;
}

void Renderer::UpdateConstantBuffer(const glm::mat4& matrix)
{
    glm::mat4 m = glm::transpose(matrix);
    memcpy(m_constantBufferDataBegin, &m, sizeof(m));
}