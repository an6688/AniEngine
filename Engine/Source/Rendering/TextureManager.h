#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <cstdint>

class RenderDevice;
class Texture;

class TextureManager
{
public:
    TextureManager();
    ~TextureManager();

    bool Initialize(RenderDevice* device);
    void Shutdown();

    // Load texture from file (cached by path)
    std::shared_ptr<Texture> LoadTexture(const std::string& filepath, bool sRGB = true);
    std::shared_ptr<Texture> LoadTexture(const std::wstring& filepath, bool sRGB = true);

    // Load texture from memory (not cached)
    std::shared_ptr<Texture> LoadTextureFromMemory(const std::string& name, const uint8_t* data, size_t dataSize, bool sRGB = true);

    // Default placeholder textures
    std::shared_ptr<Texture> GetDefaultWhiteTexture();
    std::shared_ptr<Texture> GetDefaultNormalTexture();
    std::shared_ptr<Texture> GetDefaultBlackTexture();

    void ClearCache();
    size_t GetCachedTextureCount() const { return m_textureCache.size(); }

private:
    bool CreateDefaultTextures();

    RenderDevice* m_device;
    std::unordered_map<std::string, std::weak_ptr<Texture>> m_textureCache;

    std::shared_ptr<Texture> m_defaultWhite;
    std::shared_ptr<Texture> m_defaultNormal;
    std::shared_ptr<Texture> m_defaultBlack;

    std::mutex m_cacheMutex;
};