#include "Mesh.h"
#include "RenderDevice.h"
#include <d3dx12/d3dx12.h>

Mesh::Mesh()
    : m_vertexBufferView({})
    , m_indexBufferView({})
    , m_indexCount(0)
{
}

Mesh::~Mesh()
{
    Shutdown();
}

bool Mesh::Initialize(
    RenderDevice* device,
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices)
{
    if (!device || vertices.empty() || indices.empty())
        return false;

    // Store CPU-side data
    m_vertices = vertices;
    m_indices = indices;
    m_indexCount = static_cast<uint32_t>(indices.size());

    const UINT vertexBufferSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    const UINT indexBufferSize = static_cast<UINT>(indices.size() * sizeof(uint32_t));

    // Create vertex buffer
    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

    HRESULT hr = device->GetDevice()->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)
    );

    if (FAILED(hr))
        return false;

    // Copy vertex data
    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);

    hr = m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    if (FAILED(hr))
        return false;

    memcpy(pVertexDataBegin, vertices.data(), vertexBufferSize);
    m_vertexBuffer->Unmap(0, nullptr);

    // Initialize vertex buffer view
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = vertexBufferSize;

    // Create index buffer
    CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

    hr = device->GetDevice()->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &indexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_indexBuffer)
    );

    if (FAILED(hr))
        return false;

    // Copy index data
    UINT8* pIndexDataBegin;
    hr = m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
    if (FAILED(hr))
        return false;

    memcpy(pIndexDataBegin, indices.data(), indexBufferSize);
    m_indexBuffer->Unmap(0, nullptr);

    // Initialize index buffer view
    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;  // 32-bit indices
    m_indexBufferView.SizeInBytes = indexBufferSize;

    return true;
}

void Mesh::Shutdown()
{
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_vertices.clear();
    m_indices.clear();
    m_indexCount = 0;
}