#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

class Mesh;
class RenderDevice;
class TextureManager;
class Texture;
struct Material;

// A mesh instance with its world transform
struct MeshInstance
{
    std::shared_ptr<Mesh> mesh;
    glm::mat4 transform;  // World transform from scene hierarchy
};

struct LoadedModel
{
    // Mesh instances. each has its own transform from the scene
    std::vector<MeshInstance> meshInstances;

    // Shared meshes (for instancing, mesh data without transforms)
    std::vector<std::shared_ptr<Mesh>> meshes;

    // Materials and textures
    std::vector<std::shared_ptr<Material>> materials;
    std::vector<std::shared_ptr<Texture>> textures;

    // Scene bounds (in world space, after transforms applied)
    glm::vec3 boundsMin;
    glm::vec3 boundsMax;
    glm::vec3 center;
    float size;
};

class ModelLoader
{
public:
    ModelLoader();
    ~ModelLoader();

    void SetTextureManager(TextureManager* textureManager);

    bool LoadGLTF(const char* filepath, RenderDevice* device, LoadedModel& outModel);

    // Legacy interface, loads without transforms
    bool LoadGLTF(const char* filepath, RenderDevice* device,
        std::vector<std::unique_ptr<Mesh>>& outMeshes);

private:
    TextureManager* m_textureManager;
};