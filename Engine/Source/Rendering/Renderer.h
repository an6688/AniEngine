#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <glm/glm.hpp>
#include "Mesh.h"
#include <unordered_map>

class RenderDevice;
class Mesh;
class Camera;
class Texture;
class TextureManager;
struct Material;

// Constant buffer for transform data
struct TransformConstants
{
    glm::mat4 worldViewProjection;
    glm::mat4 world;
    glm::mat4 worldInverseTranspose;
    glm::vec3 cameraPosition;
    float padding;
};

// Constant buffer for PBR material properties
struct PBRMaterialConstants
{
    // Base color
    glm::vec4 baseColorFactor;

    // Metallic-roughness
    float metallicFactor;
    float roughnessFactor;

    // Normal map
    float normalScale;

    // Occlusion
    float occlusionStrength;

    // Emissive
    glm::vec3 emissiveFactor;
    float alphaCutoff;

    // Texture presence flags
    float hasBaseColorTexture;
    float hasMetallicRoughnessTexture;
    float hasNormalTexture;
    float hasOcclusionTexture;
    float hasEmissiveTexture;

    glm::vec3 padding;
};

class Renderer
{
public:
    Renderer();
    ~Renderer();

    bool Initialize(RenderDevice* device, TextureManager* textureManager);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    // Main draw method - renders mesh with full PBR materials
    void DrawMeshTextured(Mesh* mesh, const glm::mat4& transform, Camera* camera);

    // Fallback for when no model is loaded
    void DrawCube(float deltaTime);

private:
    bool CreateCubeGeometry();
    bool CreateCubePipeline();
    bool CreatePBRPipeline();

    // Build a contiguous descriptor table for a material's textures
    uint32_t GetOrCreateMaterialDescriptorTable(Material* material);

    void UpdateConstantBuffer(const glm::mat4& matrix);

private:
    RenderDevice* m_device;
    TextureManager* m_textureManager;

    // Cube resources (fallback)
    Microsoft::WRL::ComPtr<ID3D12Resource> m_cubeVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_cubeIndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_cubeVertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_cubeIndexBufferView;
    UINT m_cubeIndexCount;

    // Cube pipeline (fallback)
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_cubeRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_cubePipelineState;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    UINT8* m_constantBufferDataBegin;

    // PBR pipeline
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_pbrRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pbrPipelineState;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_transformConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_materialConstantBuffer;
    UINT8* m_transformConstantBufferBegin;
    UINT8* m_materialConstantBufferBegin;

    // Material descriptor table cache
    // Maps material pointer to starting SRV index of its 5-texture block
    std::unordered_map<Material*, uint32_t> m_materialDescriptorCache;

    float m_cubeRotation;
};
