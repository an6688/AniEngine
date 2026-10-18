#include "Rendering/ImageProcessor.h"
#include "Rendering/RenderDevice.h"
#include <d3dx12/d3dx12.h>
#include <d3d12sdklayers.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>
using Microsoft::WRL::ComPtr;
using Pixels = std::vector<unsigned char>;
static void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

// Independent scalar CPU reference: convolution loops rather than shader's unrolled expressions.
static Pixels Reference(const Pixels& input, int width, int height, const ImageProcessingSettings& settings) {
    Pixels result = input;
    const int kernelX[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
    const int kernelY[3][3] = {{-1,-2,-1},{0,0,0},{1,2,1}};
    auto brightness = [&](int x, int y) {
        auto offset = (std::clamp(y,0,height-1)*width + std::clamp(x,0,width-1))*4;
        return (input[offset]*0.2126 + input[offset+1]*0.7152 + input[offset+2]*0.0722)/255.0;
    };
    for (int y=0; y<height; ++y) for (int x=0; x<width; ++x) {
        if (settings.mode == ImageProcessingSettings::Original ||
            (settings.compare && x+0.5 < std::clamp(settings.split,0.0f,1.0f)*width)) continue;
        double value = brightness(x,y);
        if (settings.mode == ImageProcessingSettings::Sobel) {
            double gx=0,gy=0;
            for (int j=-1; j<=1; ++j) for (int i=-1; i<=1; ++i) {
                gx += kernelX[j+1][i+1]*brightness(x+i,y+j);
                gy += kernelY[j+1][i+1]*brightness(x+i,y+j);
            }
            value = std::clamp(std::sqrt(gx*gx+gy*gy)*0.25*std::clamp(settings.strength,0.0f,8.0f),0.0,1.0);
        }
        auto byte = static_cast<unsigned char>(std::lround(value*255));
        for (int c=0; c<3; ++c) result[(y*width+x)*4+c] = byte;
    }
    return result;
}

static void SavePPM(const char* filename, const Pixels& pixels, int width, int height) {
    std::ofstream file(filename, std::ios::binary);
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (int i=0; i<width*height; ++i) file.write(reinterpret_cast<const char*>(pixels.data()+i*4),3);
}
int main() {
    try {
        WNDCLASSW wc{}; wc.lpfnWndProc=DefWindowProcW; wc.hInstance=GetModuleHandleW(nullptr);
        wc.lpszClassName=L"AniEngineImageProcessingSmoke";
        RegisterClassW(&wc);
        HWND window=CreateWindowW(wc.lpszClassName,L"Image processing smoke",WS_OVERLAPPEDWINDOW,
            0,0,512,512,nullptr,nullptr,wc.hInstance,nullptr);
        Require(window != nullptr,"Test window creation failed");
        RenderDevice device;
        Require(device.Initialize(window,512,512),"Device initialization failed");
        device.vsync=false;
        ImageProcessor processor;
        Require(processor.Initialize(&device),"Image processor/shader initialization failed");
        unsigned cases=0; int worstError=0;
        for (auto dimensions : {std::array<int,2>{1,1}, {1,9}, {11,1}, {17,13}, {128,80}, {17,13}}) {
            const int width=dimensions[0], height=dimensions[1];
            Require(processor.Prepare(width,height),"Prepare/resize failed");
            auto gpuHeap=CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            auto colorDesc=CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,width,height,1,1,1,0,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
            ComPtr<ID3D12Resource> color,upload,readback;
            Require(SUCCEEDED(device.GetDevice()->CreateCommittedResource(&gpuHeap,D3D12_HEAP_FLAG_NONE,
                &colorDesc,D3D12_RESOURCE_STATE_RENDER_TARGET,nullptr,IID_PPV_ARGS(&color))),"Color allocation failed");
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc{}; heapDesc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV; heapDesc.NumDescriptors=1;
            ComPtr<ID3D12DescriptorHeap> rtvHeap;
            Require(SUCCEEDED(device.GetDevice()->CreateDescriptorHeap(&heapDesc,IID_PPV_ARGS(&rtvHeap))),"RTV heap failed");
            auto rtv=rtvHeap->GetCPUDescriptorHandleForHeapStart();
            device.GetDevice()->CreateRenderTargetView(color.Get(),nullptr,rtv);
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{}; UINT64 size=0;
            device.GetDevice()->GetCopyableFootprints(&colorDesc,0,1,0,&footprint,nullptr,nullptr,&size);
            auto buffer=CD3DX12_RESOURCE_DESC::Buffer(size);
            auto upHeap=CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            auto readHeap=CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
            Require(SUCCEEDED(device.GetDevice()->CreateCommittedResource(&upHeap,D3D12_HEAP_FLAG_NONE,&buffer,
                D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload))),"Upload allocation failed");
            Require(SUCCEEDED(device.GetDevice()->CreateCommittedResource(&readHeap,D3D12_HEAP_FLAG_NONE,&buffer,
                D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback))),"Readback allocation failed");
            for (int pattern=0;pattern<5;++pattern) {
                Pixels pixels(width*height*4);
                for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
                    int offset=(y*width+x)*4;
                    switch(pattern) {
                    case 0: pixels[offset]=30; pixels[offset+1]=100; pixels[offset+2]=220; break;
                    case 1: for(int c=0;c<3;++c) pixels[offset+c]=x>=width/2?255:0; break;
                    case 2: for(int c=0;c<3;++c) pixels[offset+c]=y>=height/2?255:0; break;
                    case 3: for(int c=0;c<3;++c) pixels[offset+c]=(x==width/2 && y==height/2)?255:0; break;
                    case 4: pixels[offset]=static_cast<unsigned char>(x*17+y*13); pixels[offset+1]=static_cast<unsigned char>(x*31+y*7); pixels[offset+2]=static_cast<unsigned char>(((x/8+y/8)%2)*200); break;
                    }
                    pixels[offset+3]=static_cast<unsigned char>(100+(x+y)%156);
                }
                for (int variant=0;variant<8;++variant) {
                    ImageProcessingSettings settings;
                    settings.mode=variant==0?ImageProcessingSettings::Original:variant==1?ImageProcessingSettings::Grayscale:ImageProcessingSettings::Sobel;
                    settings.strength=variant==3?0.0f:variant==4?8.0f:1.0f;
                    settings.compare=variant>=5;
                    settings.split=variant==6?0.0f:variant==7?1.0f:0.5f;
                    void* mapped=nullptr; D3D12_RANGE noRead{0,0};
                    Require(SUCCEEDED(upload->Map(0,&noRead,&mapped)),"Upload mapping failed");
                    for(int y=0;y<height;++y) std::memcpy(static_cast<char*>(mapped)+y*footprint.Footprint.RowPitch,pixels.data()+y*width*4,width*4);
                    upload->Unmap(0,nullptr);
                    device.BeginFrame();
                    auto* commands=device.GetCommandList();
                    D3D12_RESOURCE_BARRIER barrier=CD3DX12_RESOURCE_BARRIER::Transition(color.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_DEST);
                    commands->ResourceBarrier(1,&barrier);
                    auto source=CD3DX12_TEXTURE_COPY_LOCATION(upload.Get(),footprint);
                    auto target=CD3DX12_TEXTURE_COPY_LOCATION(color.Get(),0);
                    commands->CopyTextureRegion(&target,0,0,0,&source,nullptr);
                    std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter); commands->ResourceBarrier(1,&barrier);
                    processor.Process(color.Get(),rtv,settings);
                    barrier=CD3DX12_RESOURCE_BARRIER::Transition(color.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);
                    commands->ResourceBarrier(1,&barrier);
                    auto destination=CD3DX12_TEXTURE_COPY_LOCATION(readback.Get(),footprint);
                    commands->CopyTextureRegion(&destination,0,0,0,&target,nullptr);
                    std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter); commands->ResourceBarrier(1,&barrier);
                    device.BindMainRenderTargets();
                    device.EndFrame(); device.WaitForGPU();
                    D3D12_RANGE readRange{0,SIZE_T(size)};
                    Require(SUCCEEDED(readback->Map(0,&readRange,&mapped)),"Readback mapping failed");
                    Pixels actual(pixels.size());
                    for(int y=0;y<height;++y) std::memcpy(actual.data()+y*width*4,static_cast<char*>(mapped)+y*footprint.Footprint.RowPitch,width*4);
                    D3D12_RANGE noWrite{0,0}; readback->Unmap(0,&noWrite);
                    auto expected=Reference(pixels,width,height,settings);
                    for(size_t i=0;i<actual.size();++i) {
                        int error=std::abs(int(actual[i])-int(expected[i])); worstError=std::max(error,worstError);
                        if (i%4==3 || settings.mode==ImageProcessingSettings::Original)
                            Require(error==0,"Bypass and alpha must be exact");
                        if(error>1) { std::printf("Mismatch: %dx%d pattern %d variant %d byte %zu expected %u actual %u\n",width,height,pattern,variant,i,expected[i],actual[i]); throw std::runtime_error("GPU does not match CPU reference"); }
                    }
                    if(pattern==0 && settings.mode==ImageProcessingSettings::Sobel && !settings.compare)
                        for(size_t i=0;i<actual.size();i+=4) Require(actual[i]==0,"Constant image must have no edges");
                    if(width==128 && pattern==4 && variant<3) SavePPM(variant==0?"processing-original.ppm":variant==1?"processing-gray.ppm":"processing-edges.ppm",actual,width,height);
                    ++cases;
                }
            }
        }
        Require(SUCCEEDED(device.GetDevice()->GetDeviceRemovedReason()),"Device removed");
        ComPtr<ID3D12InfoQueue> info;
        if(SUCCEEDED(device.GetDevice()->QueryInterface(IID_PPV_ARGS(&info)))) {
            for(UINT64 i=0;i<info->GetNumStoredMessages();++i) {
                SIZE_T size=0; info->GetMessage(i,nullptr,&size); std::vector<unsigned char> bytes(size);
                auto* message=reinterpret_cast<D3D12_MESSAGE*>(bytes.data()); info->GetMessage(i,message,&size);
                if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR) { std::puts(message->pDescription); throw std::runtime_error("D3D12 validation error"); }
            }
        } else std::puts("D3D12 debug layer unavailable; no validation messages checked");
        processor.Shutdown(); device.Shutdown(); DestroyWindow(window);
        std::printf("PASS: %u GPU/CPU comparisons, worst byte error %d/255; borders, resize, split, strength and alpha\n",cases,worstError);
        return 0;
    } catch(const std::exception& error) { std::fprintf(stderr,"FAIL: %s\n",error.what()); return 1; }
}

