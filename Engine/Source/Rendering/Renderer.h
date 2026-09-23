#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include "LightingTypes.h"

class RenderDevice;
class Mesh;
class Camera;
class Texture;
class TextureManager;
struct Scene;
struct Material;
struct RenderSettings;


// Transform constants - register(b0)
struct TransformConstants {
    glm::mat4 worldViewProjection;
    glm::mat4 world;
    glm::mat4 worldInverseTranspose;
    glm::vec3 cameraPosition;
    float padding;
};

// Material constants - register(b1)
struct PBRMaterialConstants {
    glm::vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    glm::vec3 emissiveFactor;
    float alphaCutoff;
    float hasBaseColorTexture;
    float hasMetallicRoughnessTexture;
    float hasNormalTexture;
    float hasOcclusionTexture;
    float hasEmissiveTexture;
    glm::vec3 padding;
};

// High-level renderer - owns pipelines, draws geometry
class Renderer {
public:
    Renderer();
    ~Renderer();

    bool Initialize(RenderDevice* device, TextureManager* textureManager);
    void Shutdown();

    void BeginFrame();
    void EndFrame();
    void RenderDirectionalShadow(const Scene* scene);
    ID3D12Resource* GetShadowMap() const { return m_shadowMap.Get(); }

    void DrawMeshTextured(Mesh* mesh, const glm::mat4& transform, Camera* camera);
    void DrawCube(float deltaTime);

    void SetRenderSettings(const RenderSettings* settings) { m_renderSettings = settings; }
    void UpdateLightingFromScene(const Scene* scene);

private:
    bool CreateCubeGeometry();
    bool CreateSimplePipeline();
    bool CreatePBRPipeline();
    bool CreateLightingConstantBuffer();
    bool CreateShadowResources();

    uint32_t GetOrCreateMaterialDescriptorTable(Material* material);
    void UpdateConstantBuffer(const glm::mat4& matrix);

private:
    RenderDevice* m_device;
    TextureManager* m_textureManager;
    LightingConstantBuffer m_lightingCB;

    const RenderSettings* m_renderSettings;

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
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pbrPipelineStateWireframe;

    // Transform uploads partitioned by frame-in-flight
    Microsoft::WRL::ComPtr<ID3D12Resource> m_transformConstantBuffer;
    UINT8* m_transformConstantBufferBegin;
    UINT m_currentTransformOffset;

    // Material uploads partitioned by frame-in-flight
    Microsoft::WRL::ComPtr<ID3D12Resource> m_materialConstantBuffer;
    UINT8* m_materialConstantBufferBegin;
    UINT m_currentMaterialOffset;

    // Lighting constant buffer
    Microsoft::WRL::ComPtr<ID3D12Resource> m_lightingConstantBuffer;
    UINT8* m_lightingConstantBufferBegin;

    // Constants
    static const UINT MAX_INSTANCES_PER_FRAME = 1024;
    static const UINT CB_ALIGNMENT = 256;

    // Material descriptor cache
    std::unordered_map<Material*, uint32_t> m_materialDescriptorCache;

    static constexpr UINT ShadowResolution = 2048;
    static constexpr UINT LightingStride = (sizeof(LightingConstantBuffer) + 255) & ~255u;
    struct ShadowConstants {
        glm::mat4 lightViewProjection = glm::mat4(1.0f);
        float lightIndex = -1.0f;
        float depthBias = 0.001f;
        float slopeBias = 0.003f;
        float texelSize = 1.0f / ShadowResolution;
        glm::vec4 options = glm::vec4(0.0f);
    };
    static_assert(sizeof(ShadowConstants) == 96, "Shadow constants must match HLSL");
    Microsoft::WRL::ComPtr<ID3D12Resource> m_shadowMap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_shadowDSVHeap;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_shadowRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_shadowPipeline;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_shadowConstants;
    UINT8* m_shadowConstantsBegin = nullptr;
    uint32_t m_shadowSRV = 0;
    float m_cubeRotation;
};