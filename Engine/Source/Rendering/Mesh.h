#pragma once

#include "VertexFormats.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <memory>
#include <glm/glm.hpp>

struct Material;

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
    void SetMaterial(std::shared_ptr<Material> material) { m_material = material; }
    std::shared_ptr<Material> GetMaterial() const { return m_material; }

    void SetTransform(const glm::mat4& transform) { m_transform = transform; }
    const glm::mat4& GetTransform() const { return m_transform; }

    // Local bounds (set during loading, used for world space bounds calculation)
    void SetLocalBounds(const glm::vec3& min, const glm::vec3& max)
    {
        m_localBoundsMin = min;
        m_localBoundsMax = max;
    }
    void GetLocalBounds(glm::vec3& outMin, glm::vec3& outMax) const
    {
        outMin = m_localBoundsMin;
        outMax = m_localBoundsMax;
    }

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
    std::shared_ptr<Material> m_material;
    glm::mat4 m_transform = glm::mat4(1.0f);

    // Local space bounding box
    glm::vec3 m_localBoundsMin = glm::vec3(0.0f);
    glm::vec3 m_localBoundsMax = glm::vec3(0.0f);
};