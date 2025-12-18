#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <glm/glm.hpp>
#include "Mesh.h"

class RenderDevice;
class Mesh;
class Camera;
class Texture;
struct Material;

// Constant buffer structure for textured rendering
struct TexturedConstants
{
    glm::mat4 worldViewProjection;
    glm::mat4 world;
    glm::vec3 cameraPosition;
    float padding;
};

struct MaterialConstants
{
    glm::vec4 baseColorFactor;
    float hasBaseColorTexture;
    glm::vec3 padding;
};

class Renderer
{
public:
    Renderer();
    ~Renderer();

    bool Initialize(RenderDevice* device);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    // Draw methods
    void DrawCube(float deltaTime);
    void DrawMeshTextured(Mesh* mesh, const glm::mat4& transform, Camera* camera);

private:
    bool CreateCubeGeometry();
    bool CreateCubePipeline();

    // New: Create pipeline for textured rendering
    bool CreateTexturedPipeline();

    void UpdateConstantBuffer(const glm::mat4& matrix);

private:
    RenderDevice* m_device;

    // Cube resources
    Microsoft::WRL::ComPtr<ID3D12Resource> m_cubeVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_cubeIndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_cubeVertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_cubeIndexBufferView;
    UINT m_cubeIndexCount;

    // Cube pipeline
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_cubeRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_cubePipelineState;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    UINT8* m_constantBufferDataBegin;

    // Textured pipeline (NEW)
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_texturedRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_texturedPipelineState;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_texturedConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_materialConstantBuffer;
    UINT8* m_texturedConstantBufferBegin;
    UINT8* m_materialConstantBufferBegin;

    float m_cubeRotation;
};
