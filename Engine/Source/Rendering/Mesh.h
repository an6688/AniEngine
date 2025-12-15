#pragma once

#include "VertexFormats.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <string>

// Forward declaration
class RenderDevice;

// Mesh - represents a single piece of geometry
// DEPENDS ON: RenderDevice (for creating GPU buffers)
class Mesh
{
public:
    Mesh();
    ~Mesh();

    // Initialize mesh with vertex and index data
    bool Initialize(
        RenderDevice* device,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices
    );

    // Cleanup GPU resources
    void Shutdown();

    // Get buffer views for rendering
    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return m_vertexBufferView; }
    const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return m_indexBufferView; }

    // Get index count for draw calls
    uint32_t GetIndexCount() const { return m_indexCount; }

    // Check if mesh is valid
    bool IsValid() const { return m_vertexBuffer && m_indexBuffer; }

    // Optional: Get CPU-side data (for physics, etc.)
    const std::vector<Vertex>& GetVertices() const { return m_vertices; }
    const std::vector<uint32_t>& GetIndices() const { return m_indices; }

private:
    // GPU resources
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

    uint32_t m_indexCount;

    // CPU-side data (optional, for later use)
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
};