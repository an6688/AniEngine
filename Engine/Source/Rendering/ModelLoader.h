#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>

class RenderDevice;
class Mesh;
class Texture;
class TextureManager;
struct Material;

// Helper struct for loaded model data
struct LoadedModel
{
    std::vector<std::unique_ptr<Mesh>> meshes;
    std::vector<std::shared_ptr<Material>> materials;
    std::vector<std::shared_ptr<Texture>> textures;
};

class ModelLoader
{
public:
    ModelLoader();
    ~ModelLoader();

    // Set texture manager for loading/caching textures
    void SetTextureManager(TextureManager* textureManager);

    // Load a complete model with meshes, materials, and textures
    bool LoadGLTF(const char* filepath, RenderDevice* device, LoadedModel& outModel);

    // Legacy overload for backwards compatibility (meshes only)
    bool LoadGLTF(const char* filepath, RenderDevice* device,
        std::vector<std::unique_ptr<Mesh>>& outMeshes);

private:
    TextureManager* m_textureManager;
};