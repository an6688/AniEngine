#include "TextureManager.h"
#include "Texture.h"
#include "RenderDevice.h"
#include <filesystem>

TextureManager::TextureManager()
    : m_device(nullptr)
{
}

TextureManager::~TextureManager()
{
    Shutdown();
}

bool TextureManager::Initialize(RenderDevice* device)
{
    if (!device)
        return false;

    m_device = device;

    if (!CreateDefaultTextures())
        return false;

    return true;
}

void TextureManager::Shutdown()
{
    ClearCache();

    m_defaultWhite.reset();
    m_defaultNormal.reset();
    m_defaultBlack.reset();

    m_device = nullptr;
}

std::shared_ptr<Texture> TextureManager::LoadTexture(const std::string& filepath, bool sRGB)
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    std::filesystem::path normalizedPath = std::filesystem::absolute(filepath);
    std::string cacheKey = normalizedPath.string();

    auto it = m_textureCache.find(cacheKey);
    if (it != m_textureCache.end())
    {
        auto texture = it->second.lock();
        if (texture)
        {
            return texture;
        }
        m_textureCache.erase(it);
    }

    auto texture = std::make_shared<Texture>();
    if (!texture->LoadFromFile(m_device, filepath, sRGB))
    {
        OutputDebugStringA(("Failed to load texture: " + filepath + "\n").c_str());
        return GetDefaultWhiteTexture();
    }

    m_textureCache[cacheKey] = texture;

    return texture;
}

std::shared_ptr<Texture> TextureManager::LoadTexture(const std::wstring& filepath, bool sRGB)
{
    std::filesystem::path path(filepath);
    return LoadTexture(path.string(), sRGB);
}

std::shared_ptr<Texture> TextureManager::LoadTextureFromMemory(const std::string& name, const uint8_t* data, size_t dataSize, bool sRGB)
{
    auto texture = std::make_shared<Texture>();
    if (!texture->LoadFromMemory(m_device, data, dataSize, sRGB))
    {
        OutputDebugStringA(("Failed to load embedded texture: " + name + "\n").c_str());
        return GetDefaultWhiteTexture();
    }

    return texture;
}

std::shared_ptr<Texture> TextureManager::GetDefaultWhiteTexture()
{
    return m_defaultWhite;
}

std::shared_ptr<Texture> TextureManager::GetDefaultNormalTexture()
{
    return m_defaultNormal;
}

std::shared_ptr<Texture> TextureManager::GetDefaultBlackTexture()
{
    return m_defaultBlack;
}

void TextureManager::ClearCache()
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_textureCache.clear();
}

bool TextureManager::CreateDefaultTextures()
{
    // 1x1 white texture
    {
        m_defaultWhite = std::make_shared<Texture>();
        uint32_t whitePixel = 0xFFFFFFFF;

        TextureDesc desc;
        desc.width = 1;
        desc.height = 1;
        desc.mipLevels = 1;
        desc.format = TextureFormat::RGBA8_UNORM;
        desc.debugName = L"DefaultWhite";

        if (!m_defaultWhite->Create(m_device, desc, &whitePixel, sizeof(whitePixel)))
            return false;
    }

    // 1x1 default normal (flat surface)
    {
        m_defaultNormal = std::make_shared<Texture>();
        uint32_t normalPixel = 0xFFFF8080;  // R=128, G=128, B=255, A=255

        TextureDesc desc;
        desc.width = 1;
        desc.height = 1;
        desc.mipLevels = 1;
        desc.format = TextureFormat::RGBA8_UNORM;
        desc.debugName = L"DefaultNormal";

        if (!m_defaultNormal->Create(m_device, desc, &normalPixel, sizeof(normalPixel)))
            return false;
    }

    // 1x1 black texture
    {
        m_defaultBlack = std::make_shared<Texture>();
        uint32_t blackPixel = 0xFF000000;

        TextureDesc desc;
        desc.width = 1;
        desc.height = 1;
        desc.mipLevels = 1;
        desc.format = TextureFormat::RGBA8_UNORM;
        desc.debugName = L"DefaultBlack";

        if (!m_defaultBlack->Create(m_device, desc, &blackPixel, sizeof(blackPixel)))
            return false;
    }

    return true;
}