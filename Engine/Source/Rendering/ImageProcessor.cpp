#include "ImageProcessor.h"
#include "RenderDevice.h"
#include "Shader.h"
#include <d3dx12/d3dx12.h>
#include <algorithm>
#include <cassert>

bool ImageProcessor::Initialize(RenderDevice* device) {
    m_device = device;
    CD3DX12_DESCRIPTOR_RANGE range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_ROOT_PARAMETER parameters[2];
    parameters[0].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL);
    parameters[1].InitAsConstants(6, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    CD3DX12_ROOT_SIGNATURE_DESC rootDesc(2, parameters, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    Microsoft::WRL::ComPtr<ID3DBlob> signature, errors;
    if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors))) return false;
    auto* gpu = device->GetDevice();
    if (FAILED(gpu->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_root)))) return false;
    Shader vs, ps;
    if (!vs.CompileFromFile(L"Shaders/ImageProcessing.hlsl", "VSMain", "vs_5_1") ||
        !ps.CompileFromFile(L"Shaders/ImageProcessing.hlsl", "PSMain", "ps_5_1")) return false;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline = {};
    pipeline.pRootSignature = m_root.Get();
    pipeline.VS = vs.GetBytecode(); pipeline.PS = ps.GetBytecode();
    pipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeline.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pipeline.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pipeline.DepthStencilState.DepthEnable = FALSE;
    pipeline.DepthStencilState.StencilEnable = FALSE;
    pipeline.SampleMask = UINT_MAX;
    pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline.NumRenderTargets = 1;
    pipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipeline.SampleDesc.Count = 1;
    if (FAILED(gpu->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&m_pipeline)))) return false;
    D3D12_DESCRIPTOR_HEAP_DESC heap = {};
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap.NumDescriptors = 1;
    heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    return SUCCEEDED(gpu->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&m_srvHeap)));
}

bool ImageProcessor::Prepare(UINT width, UINT height) {
    if (!m_device || !m_pipeline || !width || !height) return false;
    if (m_source && m_width == width && m_height == height) return true;
    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
    Microsoft::WRL::ComPtr<ID3D12Resource> source;
    if (FAILED(m_device->GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
        &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&source)))) return false;
    // Both the old texture and its descriptor may still be referenced by queued frames.
    m_device->WaitForGPU();
    m_source = source;
    m_source->SetName(L"Image processing scene copy");
    m_width = width; m_height = height;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = desc.Format;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    m_device->GetDevice()->CreateShaderResourceView(m_source.Get(), &srv, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}

void ImageProcessor::Process(ID3D12Resource* target, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
    const ImageProcessingSettings& settings) {
    if (settings.mode == ImageProcessingSettings::Original) return;
    assert(m_source && target && target->GetDesc().Width == m_width && target->GetDesc().Height == m_height);
    auto* commands = m_device->GetCommandList();
    D3D12_RESOURCE_BARRIER barriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(m_source.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST)
    };
    commands->ResourceBarrier(2, barriers);
    commands->CopyResource(m_source.Get(), target);
    std::swap(barriers[0].Transition.StateBefore, barriers[0].Transition.StateAfter);
    std::swap(barriers[1].Transition.StateBefore, barriers[1].Transition.StateAfter);
    commands->ResourceBarrier(2, barriers);
    commands->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    D3D12_VIEWPORT viewport = {0, 0, float(m_width), float(m_height), 0, 1};
    D3D12_RECT scissor = {0, 0, LONG(m_width), LONG(m_height)};
    commands->RSSetViewports(1, &viewport); commands->RSSetScissorRects(1, &scissor);
    commands->SetPipelineState(m_pipeline.Get());
    commands->SetGraphicsRootSignature(m_root.Get());
    ID3D12DescriptorHeap* heaps[] = {m_srvHeap.Get()};
    commands->SetDescriptorHeaps(1, heaps);
    commands->SetGraphicsRootDescriptorTable(0, m_srvHeap->GetGPUDescriptorHandleForHeapStart());
    struct Constants { UINT width, height, mode, compare; float strength, split; };
    const Constants constants{m_width, m_height, UINT(settings.mode), settings.compare ? 1u : 0u,
        std::clamp(settings.strength, 0.0f, 8.0f), std::clamp(settings.split, 0.0f, 1.0f)};
    static_assert(sizeof(Constants) == 24, "Must match ImageProcessing.hlsl");
    commands->SetGraphicsRoot32BitConstants(1, 6, &constants, 0);
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands->DrawInstanced(3, 1, 0, 0);
}
void ImageProcessor::Shutdown() {
    m_source.Reset(); m_srvHeap.Reset(); m_pipeline.Reset(); m_root.Reset();
    m_width = m_height = 0; m_device = nullptr;
}
