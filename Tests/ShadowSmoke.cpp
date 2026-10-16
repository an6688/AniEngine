#include "Rendering/RenderDevice.h"
#include "Rendering/Renderer.h"
#include "Rendering/TextureManager.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/Camera.h"
#include "Core/ImGuiManager.h"
#include "Scene/Scene.h"
#include <imgui.h>
#include <d3d12sdklayers.h>
#include <d3dx12/d3dx12.h>
#include <cstdio>
#include <stdexcept>
#include <vector>

using Microsoft::WRL::ComPtr;
static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main() {
    try {
        WNDCLASSW windowClass = {};
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = L"AniEngineShadowSmoke";
        RegisterClassW(&windowClass);
        // Hidden test window: no focus changes or visible application launch.
        HWND window = CreateWindowW(windowClass.lpszClassName, L"Shadow smoke",
            WS_OVERLAPPEDWINDOW, 0, 0, 512, 512, nullptr, nullptr, windowClass.hInstance, nullptr);
        Require(window != nullptr, "CreateWindow failed");
        RenderDevice device;
        Require(device.Initialize(window, 512, 512), "Device initialization failed");
        TextureManager textures;
        Require(textures.Initialize(&device), "Texture initialization failed");
        Renderer renderer;
        Require(renderer.Initialize(&device, &textures), "Renderer/shader initialization failed");
        RenderSettings settings;
        renderer.SetRenderSettings(&settings);
        ImGuiManager ui;
        Require(ui.Initialize(window, &device), "ImGui DX12 initialization failed");
        ui.SetShadowMap(renderer.GetShadowMap());
        Camera camera;
        camera.Initialize(1.0f);
        camera.SetDistance(12.0f);
        camera.Orbit(0.6f, 0.5f);
        Scene scene;
        SceneLight light;
        light.direction = glm::normalize(glm::vec3(0.5f, -1.0f, 0.4f));
        light.castsShadows = true;
        light.intensity = 3.0f;
        scene.lights.push_back(light);
        auto addPlane = [&](float halfSize, float y) {
            auto object = std::make_unique<SceneObject>();
            object->boundsMin = {-halfSize, y, -halfSize};
            object->boundsMax = {halfSize, y, halfSize};
            std::vector<Vertex> vertices;
            for (auto position : {glm::vec3(-halfSize,y,-halfSize), glm::vec3(-halfSize,y,halfSize),
                glm::vec3(halfSize,y,halfSize), glm::vec3(halfSize,y,-halfSize)})
                vertices.push_back({position, {0,1,0}, {0,0}, {1,1,1,1}});
            auto mesh = std::make_shared<Mesh>();
            Require(mesh->Initialize(&device, vertices, {0,1,2,0,2,3}), "Mesh initialization failed");
            auto material = std::make_shared<Material>();
            material->metallicFactor = 0.0f;
            material->roughnessFactor = 0.8f;
            mesh->SetMaterial(material);
            object->meshInstances.push_back({mesh, glm::mat4(1.0f)});
            scene.objects.push_back(std::move(object));
        };
        addPlane(5.0f, 0.0f);
        addPlane(1.0f, 1.5f);

        auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,
            512, 512, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        auto gpuHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ComPtr<ID3D12Resource> color, readback;
        Require(SUCCEEDED(device.GetDevice()->CreateCommittedResource(&gpuHeap, D3D12_HEAP_FLAG_NONE,
            &colorDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&color))), "Color target failed");
        ComPtr<ID3D12DescriptorHeap> rtvHeap;
        D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
        rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvDesc.NumDescriptors = 1;
        Require(SUCCEEDED(device.GetDevice()->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&rtvHeap))), "RTV heap failed");
        auto rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        device.GetDevice()->CreateRenderTargetView(color.Get(), nullptr, rtv);
        ComPtr<ID3D12Resource> depthTarget;
        ComPtr<ID3D12DescriptorHeap> depthHeap;
        auto depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT,
            512, 512, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        Require(SUCCEEDED(device.GetDevice()->CreateCommittedResource(&gpuHeap, D3D12_HEAP_FLAG_NONE,
            &depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, nullptr, IID_PPV_ARGS(&depthTarget))), "Depth target failed");
        auto dh = rtvDesc; dh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        Require(SUCCEEDED(device.GetDevice()->CreateDescriptorHeap(&dh, IID_PPV_ARGS(&depthHeap))), "DSV heap failed");
        auto dsv = depthHeap->GetCPUDescriptorHandleForHeapStart();
        device.GetDevice()->CreateDepthStencilView(depthTarget.Get(), nullptr, dsv);
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
        UINT64 bytes = 0;
        device.GetDevice()->GetCopyableFootprints(&colorDesc, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
        auto readHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
        auto readDesc = CD3DX12_RESOURCE_DESC::Buffer(bytes);
        Require(SUCCEEDED(device.GetDevice()->CreateCommittedResource(&readHeap, D3D12_HEAP_FLAG_NONE,
            &readDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback))), "Readback allocation failed");
        auto render = [&]() {
            device.cpuFrameStart = ProfileNowMs();
            device.cpuRecordStart = device.cpuFrameStart;
            device.BeginFrame();
            renderer.BeginFrame();
            device.Timestamp(0);
            renderer.RenderDirectionalShadow(&scene);
            device.Timestamp(1);
            renderer.UpdateLightingFromScene(&scene);
            auto* commands = device.GetCommandList();
            commands->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
            commands->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
            const float clear[] = {0,0,0,1};
            commands->ClearRenderTargetView(rtv, clear, 0, nullptr);
            for (const auto& object : scene.objects) if (object->isVisible)
                renderer.DrawMeshTextured(object->meshInstances[0].mesh.get(), glm::mat4(1), &camera);
            device.Timestamp(2);
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(color.Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
            commands->ResourceBarrier(1, &barrier);
            auto destination = CD3DX12_TEXTURE_COPY_LOCATION(readback.Get(), footprint);
            auto source = CD3DX12_TEXTURE_COPY_LOCATION(color.Get(), 0);
            commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
            barrier = CD3DX12_RESOURCE_BARRIER::Transition(color.Get(),
                D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            commands->ResourceBarrier(1, &barrier);
            device.BindMainRenderTargets();
            ui.BeginFrame();
            ImGui::Begin("Smoke test");
            ImGui::TextUnformatted("Font upload and rendering");
            ImGui::End();
            ui.Render();
            device.Timestamp(3);
            device.EndFrame();
            device.WaitForGPU();
            void* mapped = nullptr;
            CD3DX12_RANGE range(0, SIZE_T(bytes));
            Require(SUCCEEDED(readback->Map(0, &range, &mapped)), "Readback map failed");
            std::vector<unsigned char> pixels(512 * 512);
            for (int y=0; y<512; ++y) for (int x=0; x<512; ++x)
                pixels[y*512+x] = static_cast<unsigned char*>(mapped)[y*footprint.Footprint.RowPitch+x*4];
            CD3DX12_RANGE noWrite(0,0);
            readback->Unmap(0, &noWrite);
            return pixels;
        };
        settings.directionalShadows = false;
        auto before = render();
        settings.directionalShadows = true;
        auto after = render();
        size_t darker = 0;
        for (size_t i=0; i<before.size(); ++i) if (int(before[i])-int(after[i])>10) ++darker;
        std::printf("Shadow toggle: %zu pixels darkened\n", darker);
        Require(darker>100, "No visible shadow was rendered");
        scene.lights[0].castsShadows = false;
        Require(render()==before, "Cast Shadows toggle did not restore unshadowed image");
        scene.lights[0].castsShadows = true;
        auto casterMaterial = scene.objects[1]->meshInstances[0].mesh->GetMaterial();
        casterMaterial->alphaMode = Material::AlphaMode::Mask;
        casterMaterial->baseColorFactor.a = 0.0f;
        auto masked = render();
        settings.directionalShadows = false;
        Require(render()==masked, "Fully masked caster still produced a shadow");
        casterMaterial->baseColorFactor.a = 1.0f;
        casterMaterial->alphaMode = Material::AlphaMode::Opaque;
        settings.directionalShadows = true;
        settings.shadowFiltering = false;
        render();
        scene.lights[0].direction = {0,-1,0};
        render();
        for (auto& object : scene.objects) object->isVisible = false;
        render();
        scene.objects.clear();
        render();
        Require(device.HasGpuTimings(), "GPU timestamps unavailable in smoke test");
        device.vsync = false;
        render();
        Require(SUCCEEDED(device.GetLastPresentResult()), "VSync-off Present failed");
        Require(device.GetLastPresentFlags() == (device.SupportsTearing() ? DXGI_PRESENT_ALLOW_TEARING : 0), "Incorrect VSync-off flags");
        device.OnResize(640, 480);
        render();
        Require(SUCCEEDED(device.GetLastPresentResult()), "VSync-off Present failed after resize");
        device.OnResize(512, 512);
        device.vsync = true;
        render();
        Require(SUCCEEDED(device.GetLastPresentResult()) && device.GetLastPresentFlags() == 0, "VSync-on flags invalid");
        device.vsync = false;
        std::printf("Windowed tearing supported: %s\n", device.SupportsTearing() ? "yes" : "no");
        device.profile.Start();
        for (unsigned i = 0; i < 725; ++i) render();
        Require(device.profile.samples.size() == 600 && !device.profile.capturing, "Capture did not stop at 600 samples");
        for (const auto& sample : device.profile.samples) {
            Require(std::isfinite(sample.frameMs) && sample.frameMs >= 0, "Invalid CPU timing");
            for (double ms : sample.gpuMs) Require(std::isfinite(ms) && ms >= 0, "Invalid GPU timing");
        }
        Require(ProfilePercentile({1, 2, 3, 4, 100}, 0.5) == 3, "Median calculation");
        Require(ProfilePercentile({1, 2, 3, 4, 100}, 0.99) == 100, "Tail calculation");
        Require(SUCCEEDED(device.GetDevice()->GetDeviceRemovedReason()), "Device was removed");
        ComPtr<ID3D12InfoQueue> info;
        if (SUCCEEDED(device.GetDevice()->QueryInterface(IID_PPV_ARGS(&info)))) {
            size_t errors=0;
            for (UINT64 i=0; i<info->GetNumStoredMessages(); ++i) {
                SIZE_T size=0; info->GetMessage(i,nullptr,&size);
                std::vector<unsigned char> data(size);
                auto* message=reinterpret_cast<D3D12_MESSAGE*>(data.data());
                info->GetMessage(i,message,&size);
                if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
                    std::puts(message->pDescription); ++errors;
                }
            }
            Require(errors==0, "D3D12 validation reported errors");
        } else std::puts("D3D12 debug layer unavailable; validation messages not checked");
        ui.Shutdown();
        renderer.Shutdown();
        textures.Shutdown();
        device.Shutdown();
        DestroyWindow(window);
        std::puts("PASS: shadows, toggles, vertical light, empty scene, and ImGui initialization");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr,"FAIL: %s\n",error.what());
        return 1;
    }
}
