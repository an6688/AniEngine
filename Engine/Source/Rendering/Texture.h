#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <cstdint>

class RenderDevice;

enum class TextureFormat
{
    RGBA8_UNORM,
    RGBA8_SRGB,
    BGRA8_UNORM,
    BGRA8_SRGB,
    R8_UNORM,
    RG8_UNORM,
    Unknown
};

struct TextureDesc
{
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t mipLevels = 1;
    TextureFormat format = TextureFormat::RGBA8_UNORM;
    std::wstring debugName;
};

class Texture
{
public:
    Texture();
    ~Texture();

    // Non-copyable
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Movable
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // Create from raw pixel data
    bool Create(RenderDevice* device, const TextureDesc& desc, const void* initialData, size_t rowPitch);

    // Create from file
    bool LoadFromFile(RenderDevice* device, const std::wstring& filepath, bool sRGB = true);
    bool LoadFromFile(RenderDevice* device, const std::string& filepath, bool sRGB = true);

    // Create from memory (for embedded glTF textures)
    bool LoadFromMemory(RenderDevice* device, const uint8_t* data, size_t dataSize, bool sRGB = true);

    void Release();

    // Accessors
    ID3D12Resource* GetResource() const { return m_resource.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return m_srvHandle; }
    uint32_t GetSRVIndex() const { return m_srvIndex; }

    uint32_t GetWidth() const { return m_desc.width; }
    uint32_t GetHeight() const { return m_desc.height; }
    TextureFormat GetFormat() const { return m_desc.format; }

    bool IsValid() const { return m_resource != nullptr; }

private:
    bool CreateSRV(RenderDevice* device);
    bool UploadTextureData(RenderDevice* device, const void* data, size_t rowPitch);

    static DXGI_FORMAT ToDXGIFormat(TextureFormat format);
    static uint32_t GetBytesPerPixel(TextureFormat format);

    Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffer;

    TextureDesc m_desc;
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle;
    uint32_t m_srvIndex;

    RenderDevice* m_device;
};