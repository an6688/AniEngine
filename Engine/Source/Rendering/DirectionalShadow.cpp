#include "Renderer.h"
#include "RenderDevice.h"
#include "Shader.h"
#include "Mesh.h"
#include "Material.h"
#include "Core/ImGuiManager.h"
#include "Scene/Scene.h"
#include <d3dx12/d3dx12.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

bool Renderer::CreateShadowResources() {
    auto* device = m_device->GetDevice();
    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
    auto desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS,
        ShadowResolution, ShadowResolution, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    D3D12_CLEAR_VALUE clear = {};
    clear.Format = DXGI_FORMAT_D32_FLOAT;
    clear.DepthStencil.Depth = 1.0f;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear, IID_PPV_ARGS(&m_shadowMap)))) return false;
    m_shadowMap->SetName(L"Directional shadow depth");

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    heapDesc.NumDescriptors = 1;
    if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_shadowDSVHeap)))) return false;
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
    dsv.Format = DXGI_FORMAT_D32_FLOAT;
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(m_shadowMap.Get(), &dsv, m_shadowDSVHeap->GetCPUDescriptorHandleForHeapStart());
    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = DXGI_FORMAT_R32_FLOAT;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    auto handle = m_device->AllocateSRV(m_shadowSRV);
    if (!handle.ptr) return false;
    device->CreateShaderResourceView(m_shadowMap.Get(), &srv, handle);

    CD3DX12_DESCRIPTOR_RANGE range;
    range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_ROOT_PARAMETER params[2];
    params[0].InitAsConstants(20, 0);
    params[1].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL);
    CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
    CD3DX12_ROOT_SIGNATURE_DESC rootDesc;
    rootDesc.Init(2, params, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    Microsoft::WRL::ComPtr<ID3DBlob> signature, error;
    if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
        if (error) OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        return false;
    }
    if (FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
        IID_PPV_ARGS(&m_shadowRootSignature)))) return false;
    Shader vs, ps;
    if (!vs.CompileFromFile(L"Shaders/Shadow.hlsl", "VSMain", "vs_5_1") ||
        !ps.CompileFromFile(L"Shaders/Shadow.hlsl", "PSMain", "ps_5_1")) return false;
    D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = m_shadowRootSignature.Get();
    pso.VS = vs.GetBytecode();
    pso.PS = ps.GetBytecode();
    pso.InputLayout = {layout, _countof(layout)};
    pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    // Two-sided depth supports thin imported surfaces without losing their shadows.
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count = 1;
    if (FAILED(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_shadowPipeline)))) return false;
    CD3DX12_HEAP_PROPERTIES upload(D3D12_HEAP_TYPE_UPLOAD);
    auto buffer = CD3DX12_RESOURCE_DESC::Buffer(CB_ALIGNMENT * RenderDevice::FrameBufferCount);
    if (FAILED(device->CreateCommittedResource(&upload, D3D12_HEAP_FLAG_NONE, &buffer,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_shadowConstants)))) return false;
    CD3DX12_RANGE noRead(0, 0);
    return SUCCEEDED(m_shadowConstants->Map(0, &noRead, reinterpret_cast<void**>(&m_shadowConstantsBegin)));
}

void Renderer::RenderDirectionalShadow(const Scene* scene) {
    ShadowConstants constants;
    constants.depthBias = m_renderSettings ? m_renderSettings->shadowBias : 0.001f;
    constants.slopeBias = m_renderSettings ? m_renderSettings->shadowSlopeBias : 0.003f;
    constants.options.x = (!m_renderSettings || m_renderSettings->shadowFiltering) ? 1.0f : 0.0f;
    const SceneLight* selected = nullptr;
    int lightIndex = 0;
    if (scene && (!m_renderSettings || m_renderSettings->directionalShadows)) {
        for (const auto& light : scene->lights) {
            if (!light.isEnabled) continue;
            if (lightIndex >= MAX_LIGHTS) break;
            if (light.type == SceneLight::Type::Directional && light.castsShadows &&
                glm::dot(light.direction, light.direction) > 0.000001f) {
                selected = &light;
                constants.lightIndex = static_cast<float>(lightIndex);
                break;
            }
            ++lightIndex;
        }
    }
    const bool hasGeometry = scene && std::any_of(scene->objects.begin(), scene->objects.end(),
        [](const auto& object) { return object->isVisible && !object->meshInstances.empty(); });
    glm::mat4 lightVP(1.0f);
    if (selected && hasGeometry) {
        glm::vec3 low, high;
        scene->GetWorldBounds(low, high);
        const glm::vec3 center = (low + high) * 0.5f;
        const float radius = std::max(glm::length(high - low) * 0.5f, 0.5f);
        const glm::vec3 direction = glm::normalize(selected->direction);
        const glm::vec3 up = std::abs(direction.y) > 0.95f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        const glm::mat4 view = glm::lookAtRH(center - direction * (radius * 2.0f + 1.0f), center, up);
        // Explicit D3D depth convention: 0..1, independent of GLM's global defaults.
        lightVP = glm::orthoRH_ZO(-radius, radius, -radius, radius, 0.1f, radius * 4.0f + 2.0f) * view;
        constants.lightViewProjection = glm::transpose(lightVP);
    } else {
        constants.lightIndex = -1.0f;
    }
    memcpy(m_shadowConstantsBegin + m_device->GetFrameIndex() * CB_ALIGNMENT, &constants, sizeof(constants));

    auto* commands = m_device->GetCommandList();
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    commands->ResourceBarrier(1, &barrier);
    auto depth = m_shadowDSVHeap->GetCPUDescriptorHandleForHeapStart();
    commands->OMSetRenderTargets(0, nullptr, FALSE, &depth);
    commands->ClearDepthStencilView(depth, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    D3D12_VIEWPORT viewport = {0, 0, float(ShadowResolution), float(ShadowResolution), 0, 1};
    D3D12_RECT scissor = {0, 0, LONG(ShadowResolution), LONG(ShadowResolution)};
    commands->RSSetViewports(1, &viewport);
    commands->RSSetScissorRects(1, &scissor);
    commands->SetPipelineState(m_shadowPipeline.Get());
    commands->SetGraphicsRootSignature(m_shadowRootSignature.Get());
    ID3D12DescriptorHeap* heaps[] = {m_device->GetSRVHeap()};
    commands->SetDescriptorHeaps(1, heaps);
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (constants.lightIndex >= 0.0f) {
        UINT submitted = 0;
        for (const auto& object : scene->objects) {
            if (!object->isVisible) continue;
            for (size_t i = 0; i < object->meshInstances.size(); ++i) {
                const auto& mesh = object->meshInstances[i].mesh;
                if (!mesh || !mesh->IsValid()) continue;
                // Match the existing camera pass submission limit.
                if (submitted++ >= MAX_INSTANCES_PER_FRAME) continue;
                auto material = mesh->GetMaterial();
                // Transparent surfaces need a separate transmittance solution.
                if (!material || material->alphaMode == Material::AlphaMode::Blend) continue;
                const uint32_t descriptors = GetOrCreateMaterialDescriptorTable(material.get());
                if (descriptors == UINT32_MAX) continue;
                struct DrawConstants { glm::mat4 matrix; glm::vec4 alpha; } draw;
                draw.matrix = glm::transpose(lightVP * object->GetMeshWorldMatrix(i));
                draw.alpha = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                if (material->alphaMode == Material::AlphaMode::Mask) {
                    draw.alpha = glm::vec4(material->baseColorFactor.a, material->alphaCutoff,
                        material->baseColorTexture ? 1.0f : 0.0f, 0.0f);
                }
                static_assert(sizeof(DrawConstants) == 80, "Shadow root constants must match HLSL");
                commands->SetGraphicsRoot32BitConstants(0, 20, &draw, 0);
                commands->SetGraphicsRootDescriptorTable(1, m_device->GetSRVGPUHandle(descriptors));
                auto vertex = mesh->GetVertexBufferView();
                auto index = mesh->GetIndexBufferView();
                commands->IASetVertexBuffers(0, 1, &vertex);
                commands->IASetIndexBuffer(&index);
                commands->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);
            }
        }
    }
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap.Get(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commands->ResourceBarrier(1, &barrier);
    m_device->BindMainRenderTargets();
}
