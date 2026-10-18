#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
class RenderDevice;
struct ImageProcessingSettings {
    enum Mode { Original = 0, Grayscale = 1, Sobel = 2 };
    int mode = Original;
    float strength = 1.0f;
    bool compare = false;
    float split = 0.5f;
};
// One fullscreen pixel pass over a copy of the scene color, before editor UI.
// Resources are serialized on the same direct queue as scene rendering.
class ImageProcessor {
public:
    bool Initialize(RenderDevice* device);
    // Call before BeginFrame. Reallocation waits for old resources to retire.
    bool Prepare(UINT width, UINT height);
    // Target must be RGBA8_UNORM, single-sample, and in RENDER_TARGET state.
    // Leaves it in that state. The caller restores any depth/viewport bindings.
    void Process(ID3D12Resource* target, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
        const ImageProcessingSettings& settings);
    void Shutdown(); // Caller must retire GPU work first.
private:
    RenderDevice* m_device = nullptr;
    UINT m_width = 0, m_height = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_source;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_root;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipeline;
};
