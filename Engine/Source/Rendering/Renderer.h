#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>

// Forward declarations
class RenderDevice;

// High-level renderer - manages renderables and draw calls
// DEPENDS ON: RenderDevice (passed as reference, not owned)
class Renderer
{
public:
    Renderer();
    ~Renderer();

    // Initialize renderer with a device
    bool Initialize(RenderDevice* device);

    // Shutdown and cleanup
    void Shutdown();

    // Begin rendering a frame
    void BeginFrame();

    // End rendering a frame
    void EndFrame();

    // Draw the test triangle
    void DrawTriangle();

    // Draw a spinning cube
    void DrawCube(float deltaTime);

private:
    // Create triangle geometry
    bool CreateTriangleGeometry();

    // Create cube geometry
    bool CreateCubeGeometry();

    // Create shaders and pipeline state for basic rendering
    bool CreateBasicPipeline();

    // Create pipeline with constant buffer support (for cube)
    bool CreateCubePipeline();

    // Update constant buffer with new matrix
    void UpdateConstantBuffer(const DirectX::XMMATRIX& matrix);

private:
    RenderDevice* m_device;  // Not owned, just a reference

    // Triangle resources
    Microsoft::WRL::ComPtr<ID3D12Resource> m_triangleVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_triangleVertexBufferView;

    // Pipeline resources
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_basicRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_basicPipelineState;

    // Cube resources
    Microsoft::WRL::ComPtr<ID3D12Resource> m_cubeVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_cubeIndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_cubeVertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_cubeIndexBufferView;
    UINT m_cubeIndexCount;

    // Cube pipeline and constant buffer
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_cubeRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_cubePipelineState;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    UINT8* m_constantBufferDataBegin;  // Mapped pointer for updating

    // Animation state
    float m_cubeRotation;
};