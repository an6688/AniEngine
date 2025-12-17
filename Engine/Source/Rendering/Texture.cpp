#include "Texture.h"
#include "RenderDevice.h"
#include <d3dx12/d3dx12.h>
#include <filesystem>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

Texture::Texture()
    : m_srvHandle{}
    , m_srvIndex(UINT32_MAX)
    , m_device(nullptr)
{
}

Texture::~Texture()
{
    Release();
}

Texture::Texture(Texture&& other) noexcept
    : m_resource(std::move(other.m_resource))
    , m_uploadBuffer(std::move(other.m_uploadBuffer))
    , m_desc(other.m_desc)
    , m_srvHandle(other.m_srvHandle)
    , m_srvIndex(other.m_srvIndex)
    , m_device(other.m_device)
{
    other.m_srvHandle = {};
    other.m_srvIndex = UINT32_MAX;
    other.m_device = nullptr;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        Release();
        m_resource = std::move(other.m_resource);
        m_uploadBuffer = std::move(other.m_uploadBuffer);
        m_desc = other.m_desc;
        m_srvHandle = other.m_srvHandle;
        m_srvIndex = other.m_srvIndex;
        m_device = other.m_device;

        other.m_srvHandle = {};
        other.m_srvIndex = UINT32_MAX;
        other.m_device = nullptr;
    }
    return *this;
}

bool Texture::Create(RenderDevice* device, const TextureDesc& desc, const void* initialData, size_t rowPitch)
{
    if (!device || !device->GetDevice())
        return false;

    Release();

    m_device = device;
    m_desc = desc;

    // Create the texture resource
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = desc.width;
    resourceDesc.Height = desc.height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = static_cast<UINT16>(desc.mipLevels);
    resourceDesc.Format = ToDXGIFormat(desc.format);
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    HRESULT hr = device->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_resource)
    );

    if (FAILED(hr))
        return false;

    if (!desc.debugName.empty())
    {
        m_resource->SetName(desc.debugName.c_str());
    }

    // Upload initial data
    if (initialData)
    {
        if (!UploadTextureData(device, initialData, rowPitch))
            return false;
    }

    // Create shader resource view
    if (!CreateSRV(device))
        return false;

    return true;
}

bool Texture::LoadFromFile(RenderDevice* device, const std::wstring& filepath, bool sRGB)
{
    std::filesystem::path path(filepath);
    return LoadFromFile(device, path.string(), sRGB);
}

bool Texture::LoadFromFile(RenderDevice* device, const std::string& filepath, bool sRGB)
{
    if (!std::filesystem::exists(filepath))
    {
        OutputDebugStringA(("Texture file not found: " + filepath + "\n").c_str());
        return false;
    }

    int width, height, channels;
    stbi_set_flip_vertically_on_load(false);

    unsigned char* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels)
    {
        OutputDebugStringA(("Failed to load texture: " + filepath + " - " + stbi_failure_reason() + "\n").c_str());
        return false;
    }

    TextureDesc desc;
    desc.width = static_cast<uint32_t>(width);
    desc.height = static_cast<uint32_t>(height);
    desc.mipLevels = 1;
    desc.format = sRGB ? TextureFormat::RGBA8_SRGB : TextureFormat::RGBA8_UNORM;

    std::filesystem::path p(filepath);
    desc.debugName = p.filename().wstring();

    size_t rowPitch = static_cast<size_t>(width) * 4;
    bool result = Create(device, desc, pixels, rowPitch);

    stbi_image_free(pixels);

    return result;
}

bool Texture::LoadFromMemory(RenderDevice* device, const uint8_t* data, size_t dataSize, bool sRGB)
{
    if (!data || dataSize == 0)
        return false;

    int width, height, channels;
    stbi_set_flip_vertically_on_load(false);

    unsigned char* pixels = stbi_load_from_memory(
        data,
        static_cast<int>(dataSize),
        &width,
        &height,
        &channels,
        STBI_rgb_alpha
    );

    if (!pixels)
    {
        OutputDebugStringA(("Failed to load texture from memory: " + std::string(stbi_failure_reason()) + "\n").c_str());
        return false;
    }

    TextureDesc desc;
    desc.width = static_cast<uint32_t>(width);
    desc.height = static_cast<uint32_t>(height);
    desc.mipLevels = 1;
    desc.format = sRGB ? TextureFormat::RGBA8_SRGB : TextureFormat::RGBA8_UNORM;
    desc.debugName = L"EmbeddedTexture";

    size_t rowPitch = static_cast<size_t>(width) * 4;
    bool result = Create(device, desc, pixels, rowPitch);

    stbi_image_free(pixels);

    return result;
}

void Texture::Release()
{
    m_resource.Reset();
    m_uploadBuffer.Reset();
    m_srvHandle = {};
    m_srvIndex = UINT32_MAX;
    m_device = nullptr;
}

bool Texture::CreateSRV(RenderDevice* device)
{
    m_srvHandle = device->AllocateSRV(m_srvIndex);

    if (m_srvHandle.ptr == 0)
        return false;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = ToDXGIFormat(m_desc.format);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = m_desc.mipLevels;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    device->GetDevice()->CreateShaderResourceView(
        m_resource.Get(),
        &srvDesc,
        m_srvHandle
    );

    return true;
}

bool Texture::UploadTextureData(RenderDevice* device, const void* data, size_t rowPitch)
{
    auto d3dDevice = device->GetDevice();

    D3D12_RESOURCE_DESC resourceDesc = m_resource->GetDesc();
    UINT64 uploadBufferSize = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT numRows;
    UINT64 rowSizeInBytes;

    d3dDevice->GetCopyableFootprints(
        &resourceDesc,
        0, 1, 0,
        &footprint,
        &numRows,
        &rowSizeInBytes,
        &uploadBufferSize
    );

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    HRESULT hr = d3dDevice->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_uploadBuffer)
    );

    if (FAILED(hr))
        return false;

    void* mappedData = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    hr = m_uploadBuffer->Map(0, &readRange, &mappedData);

    if (FAILED(hr))
        return false;

    const uint8_t* srcData = static_cast<const uint8_t*>(data);
    uint8_t* dstData = static_cast<uint8_t*>(mappedData);

    for (UINT row = 0; row < numRows; ++row)
    {
        memcpy(
            dstData + row * footprint.Footprint.RowPitch,
            srcData + row * rowPitch,
            (std::min)(rowPitch, static_cast<size_t>(rowSizeInBytes))
        );
    }

    m_uploadBuffer->Unmap(0, nullptr);

    // Get command list and allocator
    auto commandList = device->GetCommandList();
    auto commandAllocator = device->GetCurrentCommandAllocator();

    commandAllocator->Reset();
    commandList->Reset(commandAllocator, nullptr);

    CD3DX12_TEXTURE_COPY_LOCATION dst(m_resource.Get(), 0);
    CD3DX12_TEXTURE_COPY_LOCATION src(m_uploadBuffer.Get(), footprint);

    commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    commandList->ResourceBarrier(1, &barrier);

    commandList->Close();

    ID3D12CommandList* commandLists[] = { commandList };
    device->GetCommandQueue()->ExecuteCommandLists(1, commandLists);

    device->WaitForGPU();

    return true;
}

DXGI_FORMAT Texture::ToDXGIFormat(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::RGBA8_UNORM:  return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::RGBA8_SRGB:   return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TextureFormat::BGRA8_UNORM:  return DXGI_FORMAT_B8G8R8A8_UNORM;
    case TextureFormat::BGRA8_SRGB:   return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case TextureFormat::R8_UNORM:     return DXGI_FORMAT_R8_UNORM;
    case TextureFormat::RG8_UNORM:    return DXGI_FORMAT_R8G8_UNORM;
    default:                          return DXGI_FORMAT_UNKNOWN;
    }
}

uint32_t Texture::GetBytesPerPixel(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::RGBA8_UNORM:
    case TextureFormat::RGBA8_SRGB:
    case TextureFormat::BGRA8_UNORM:
    case TextureFormat::BGRA8_SRGB:
        return 4;
    case TextureFormat::RG8_UNORM:
        return 2;
    case TextureFormat::R8_UNORM:
        return 1;
    default:
        return 0;
    }
}