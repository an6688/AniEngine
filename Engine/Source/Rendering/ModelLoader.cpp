#include "ModelLoader.h"
#include "Mesh.h"
#include "Texture.h"
#include "TextureManager.h"
#include "Material.h"
#include "RenderDevice.h"
#include "VertexFormats.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <filesystem>
#include <fstream>

// Helper to convert fastgltf error to string for better debugging
static const char* GetFastGltfErrorString(fastgltf::Error error)
{
    switch (error)
    {
    case fastgltf::Error::None: return "None";
    case fastgltf::Error::InvalidPath: return "InvalidPath";
    case fastgltf::Error::MissingExtensions: return "MissingExtensions";
    case fastgltf::Error::UnknownRequiredExtension: return "UnknownRequiredExtension";
    case fastgltf::Error::InvalidJson: return "InvalidJson";
    case fastgltf::Error::InvalidGltf: return "InvalidGltf";
    case fastgltf::Error::InvalidOrMissingAssetField: return "InvalidOrMissingAssetField";
    case fastgltf::Error::InvalidGLB: return "InvalidGLB";
    case fastgltf::Error::MissingField: return "MissingField";
    case fastgltf::Error::MissingExternalBuffer: return "MissingExternalBuffer";
    case fastgltf::Error::UnsupportedVersion: return "UnsupportedVersion";
    case fastgltf::Error::InvalidURI: return "InvalidURI";
    case fastgltf::Error::InvalidFileData: return "InvalidFileData";
    default: return "Unknown";
    }
}

ModelLoader::ModelLoader()
    : m_textureManager(nullptr)
{
}

ModelLoader::~ModelLoader()
{
}

void ModelLoader::SetTextureManager(TextureManager* textureManager)
{
    m_textureManager = textureManager;
}

// Helper to get default texture when needed
static std::shared_ptr<Texture> GetDefaultTexture(TextureManager* mgr, bool sRGB)
{
    if (mgr)
    {
        return mgr->GetDefaultWhiteTexture();
    }
    return nullptr;
}

// Helper to load a texture from fastgltf
static std::shared_ptr<Texture> LoadTextureFromGltf(
    const fastgltf::Asset& asset,
    size_t textureIndex,
    const std::string& basePath,
    RenderDevice* device,
    TextureManager* textureManager,
    bool sRGB,
    std::unordered_map<size_t, std::shared_ptr<Texture>>& textureCache)
{
    // Check cache first
    auto cacheIt = textureCache.find(textureIndex);
    if (cacheIt != textureCache.end())
    {
        return cacheIt->second;
    }

    if (textureIndex >= asset.textures.size())
    {
        return GetDefaultTexture(textureManager, sRGB);
    }

    const fastgltf::Texture& gltfTexture = asset.textures[textureIndex];

    // Get the image index
    size_t imageIndex;
    if (gltfTexture.imageIndex.has_value())
    {
        imageIndex = gltfTexture.imageIndex.value();
    }
    else
    {
        return GetDefaultTexture(textureManager, sRGB);
    }

    if (imageIndex >= asset.images.size())
    {
        return GetDefaultTexture(textureManager, sRGB);
    }

    const fastgltf::Image& image = asset.images[imageIndex];
    std::shared_ptr<Texture> texture;

    // Handle different image data sources
    std::visit(fastgltf::visitor{
        [](auto& arg) {
            // Default case - unsupported
        },
        [&](const fastgltf::sources::URI& uri) {
            // External file
            std::string imagePath = basePath + "/" + std::string(uri.uri.path());

            if (textureManager)
            {
                texture = textureManager->LoadTexture(imagePath, sRGB);
            }
            else
            {
                texture = std::make_shared<Texture>();
                if (!texture->LoadFromFile(device, imagePath, sRGB))
                {
                    texture = nullptr;
                }
            }
        },
        [&](const fastgltf::sources::Array& array) {
            // Embedded data
            if (textureManager)
            {
                texture = textureManager->LoadTextureFromMemory(
                    image.name.empty() ? "embedded" : std::string(image.name),
                    reinterpret_cast<const uint8_t*>(array.bytes.data()),
                    array.bytes.size(),
                    sRGB
                );
            }
            else
            {
                texture = std::make_shared<Texture>();
                if (!texture->LoadFromMemory(
                    device,
                    reinterpret_cast<const uint8_t*>(array.bytes.data()),
                    array.bytes.size(),
                    sRGB))
                {
                    texture = nullptr;
                }
            }
        },
        [&](const fastgltf::sources::Vector& vector) {
            // Embedded data (vector variant)
            if (textureManager)
            {
                texture = textureManager->LoadTextureFromMemory(
                    image.name.empty() ? "embedded" : std::string(image.name),
                    reinterpret_cast<const uint8_t*>(vector.bytes.data()),
                    vector.bytes.size(),
                    sRGB
                );
            }
            else
            {
                texture = std::make_shared<Texture>();
                if (!texture->LoadFromMemory(
                    device,
                    reinterpret_cast<const uint8_t*>(vector.bytes.data()),
                    vector.bytes.size(),
                    sRGB))
                {
                    texture = nullptr;
                }
            }
        },
        [&](const fastgltf::sources::BufferView& bufferViewSource) {
            // Data in buffer view
            const fastgltf::BufferView& bufferView = asset.bufferViews[bufferViewSource.bufferViewIndex];
            const fastgltf::Buffer& buffer = asset.buffers[bufferView.bufferIndex];

            // Get the actual data from the buffer
            std::visit(fastgltf::visitor{
                [](auto& arg) {},
                [&](const fastgltf::sources::Array& array) {
                    const uint8_t* data = reinterpret_cast<const uint8_t*>(array.bytes.data()) + bufferView.byteOffset;
                    size_t dataSize = bufferView.byteLength;

                    if (textureManager)
                    {
                        texture = textureManager->LoadTextureFromMemory(
                            image.name.empty() ? "embedded" : std::string(image.name),
                            data, dataSize, sRGB
                        );
                    }
                    else
                    {
                        texture = std::make_shared<Texture>();
                        if (!texture->LoadFromMemory(device, data, dataSize, sRGB))
                        {
                            texture = nullptr;
                        }
                    }
                },
                [&](const fastgltf::sources::Vector& vector) {
                    const uint8_t* data = reinterpret_cast<const uint8_t*>(vector.bytes.data()) + bufferView.byteOffset;
                    size_t dataSize = bufferView.byteLength;

                    if (textureManager)
                    {
                        texture = textureManager->LoadTextureFromMemory(
                            image.name.empty() ? "embedded" : std::string(image.name),
                            data, dataSize, sRGB
                        );
                    }
                    else
                    {
                        texture = std::make_shared<Texture>();
                        if (!texture->LoadFromMemory(device, data, dataSize, sRGB))
                        {
                            texture = nullptr;
                        }
                    }
                }
            }, buffer.data);
        }
        }, image.data);

    if (!texture)
    {
        texture = GetDefaultTexture(textureManager, sRGB);
    }

    // Cache the loaded texture
    if (texture)
    {
        textureCache[textureIndex] = texture;
    }

    return texture;
}

bool ModelLoader::LoadGLTF(const char* filepath, RenderDevice* device, LoadedModel& outModel)
{
    std::filesystem::path path(filepath);

    if (!std::filesystem::exists(path))
    {
        OutputDebugStringA(("File not found: " + std::string(filepath) + "\n").c_str());
        return false;
    }

    OutputDebugStringA(("Loading glTF: " + std::string(filepath) + "\n").c_str());

    // Check what files exist in the directory (for debugging buffer issues)
    std::filesystem::path parentPath = path.parent_path();
    OutputDebugStringA(("Directory contents:\n"));
    for (const auto& entry : std::filesystem::directory_iterator(parentPath))
    {
        OutputDebugStringA(("  " + entry.path().filename().string() + "\n").c_str());
    }

    // Setup fastgltf parser with extensions support
    fastgltf::Parser parser(
        fastgltf::Extensions::KHR_materials_specular |
        fastgltf::Extensions::KHR_materials_ior |
        fastgltf::Extensions::KHR_materials_iridescence |
        fastgltf::Extensions::KHR_materials_volume |
        fastgltf::Extensions::KHR_materials_transmission |
        fastgltf::Extensions::KHR_materials_clearcoat |
        fastgltf::Extensions::KHR_materials_emissive_strength |
        fastgltf::Extensions::KHR_materials_sheen |
        fastgltf::Extensions::KHR_materials_unlit |
        fastgltf::Extensions::KHR_mesh_quantization |
        fastgltf::Extensions::KHR_lights_punctual |
        fastgltf::Extensions::KHR_texture_transform |
        fastgltf::Extensions::KHR_texture_basisu
    );

    // Configure what data to load
    constexpr auto gltfOptions =
        fastgltf::Options::LoadExternalBuffers |
        fastgltf::Options::LoadExternalImages |
        fastgltf::Options::GenerateMeshIndices |
        fastgltf::Options::DecomposeNodeMatrices;

    // Load the glTF file
    auto dataResult = fastgltf::GltfDataBuffer::FromPath(path);
    if (dataResult.error() != fastgltf::Error::None)
    {
        OutputDebugStringA(("Failed to load glTF data buffer: " +
            std::string(GetFastGltfErrorString(dataResult.error())) + "\n").c_str());
        return false;
    }

    fastgltf::GltfDataBuffer& dataBuffer = dataResult.get();
    auto type = fastgltf::determineGltfFileType(dataBuffer);
    fastgltf::Expected<fastgltf::Asset> assetResult(fastgltf::Error::None);

    if (type == fastgltf::GltfType::glTF)
    {
        assetResult = parser.loadGltf(dataBuffer, path.parent_path(), gltfOptions);
    }
    else if (type == fastgltf::GltfType::GLB)
    {
        assetResult = parser.loadGltfBinary(dataBuffer, path.parent_path(), gltfOptions);
    }
    else
    {
        OutputDebugStringA("Unknown glTF file type\n");
        return false;
    }

    if (assetResult.error() != fastgltf::Error::None)
    {
        // Provide more detailed error info
        char errorMsg[512];
        sprintf_s(errorMsg, "Failed to parse glTF: %s (code %d)\n",
            GetFastGltfErrorString(assetResult.error()),
            static_cast<int>(assetResult.error()));
        OutputDebugStringA(errorMsg);

        // For MissingExternalBuffer, try to identify which buffer is missing
        if (assetResult.error() == fastgltf::Error::MissingExternalBuffer)
        {
            OutputDebugStringA("Hint: Check that all .bin files referenced in the glTF exist in the same folder.\n");
            OutputDebugStringA("The buffer URI in the glTF must exactly match the filename (case-sensitive).\n");
        }

        return false;
    }

    fastgltf::Asset& asset = assetResult.get();
    std::string basePath = path.parent_path().string();

    // Debug output
    char msg[256];
    sprintf_s(msg, "glTF has %zu meshes, %zu materials, %zu textures, %zu images, %zu buffers\n",
        asset.meshes.size(), asset.materials.size(),
        asset.textures.size(), asset.images.size(),
        asset.buffers.size());
    OutputDebugStringA(msg);

    // Initialize bounds tracking
    outModel.boundsMin = glm::vec3(FLT_MAX);
    outModel.boundsMax = glm::vec3(-FLT_MAX);

    // Texture cache for this load operation
    std::unordered_map<size_t, std::shared_ptr<Texture>> textureCache;

    // Load materials
    outModel.materials.reserve(asset.materials.size());

    for (const auto& gltfMat : asset.materials)
    {
        auto material = std::make_shared<Material>();
        material->name = std::string(gltfMat.name);

        // Base color
        const auto& pbr = gltfMat.pbrData;

        material->baseColorFactor = glm::vec4(
            pbr.baseColorFactor[0],
            pbr.baseColorFactor[1],
            pbr.baseColorFactor[2],
            pbr.baseColorFactor[3]
        );

        if (pbr.baseColorTexture.has_value())
        {
            material->baseColorTexture = LoadTextureFromGltf(
                asset, pbr.baseColorTexture->textureIndex,
                basePath, device, m_textureManager, true, textureCache
            );
            material->baseColorTexCoord = static_cast<int>(pbr.baseColorTexture->texCoordIndex);
        }

        // Metallic-Roughness
        material->metallicFactor = pbr.metallicFactor;
        material->roughnessFactor = pbr.roughnessFactor;

        if (pbr.metallicRoughnessTexture.has_value())
        {
            material->metallicRoughnessTexture = LoadTextureFromGltf(
                asset, pbr.metallicRoughnessTexture->textureIndex,
                basePath, device, m_textureManager, false, textureCache
            );
            material->metallicRoughnessTexCoord = static_cast<int>(pbr.metallicRoughnessTexture->texCoordIndex);
        }

        // Normal map
        if (gltfMat.normalTexture.has_value())
        {
            material->normalTexture = LoadTextureFromGltf(
                asset, gltfMat.normalTexture->textureIndex,
                basePath, device, m_textureManager, false, textureCache
            );
            material->normalScale = gltfMat.normalTexture->scale;
            material->normalTexCoord = static_cast<int>(gltfMat.normalTexture->texCoordIndex);
        }

        // Occlusion
        if (gltfMat.occlusionTexture.has_value())
        {
            material->occlusionTexture = LoadTextureFromGltf(
                asset, gltfMat.occlusionTexture->textureIndex,
                basePath, device, m_textureManager, false, textureCache
            );
            material->occlusionStrength = gltfMat.occlusionTexture->strength;
            material->occlusionTexCoord = static_cast<int>(gltfMat.occlusionTexture->texCoordIndex);
        }

        // Emissive
        material->emissiveFactor = glm::vec3(
            gltfMat.emissiveFactor[0],
            gltfMat.emissiveFactor[1],
            gltfMat.emissiveFactor[2]
        );

        if (gltfMat.emissiveTexture.has_value())
        {
            material->emissiveTexture = LoadTextureFromGltf(
                asset, gltfMat.emissiveTexture->textureIndex,
                basePath, device, m_textureManager, true, textureCache
            );
            material->emissiveTexCoord = static_cast<int>(gltfMat.emissiveTexture->texCoordIndex);
        }

        // Alpha mode
        if (gltfMat.alphaMode == fastgltf::AlphaMode::Mask)
        {
            material->alphaMode = Material::AlphaMode::Mask;
            material->alphaCutoff = gltfMat.alphaCutoff;
        }
        else if (gltfMat.alphaMode == fastgltf::AlphaMode::Blend)
        {
            material->alphaMode = Material::AlphaMode::Blend;
        }
        else
        {
            material->alphaMode = Material::AlphaMode::Opaque;
        }

        material->doubleSided = gltfMat.doubleSided;

        outModel.materials.push_back(material);
    }

    // Collect all loaded textures
    for (auto& pair : textureCache)
    {
        outModel.textures.push_back(pair.second);
    }

    // Load meshes
    for (const auto& gltfMesh : asset.meshes)
    {
        for (const auto& primitive : gltfMesh.primitives)
        {
            if (primitive.type != fastgltf::PrimitiveType::Triangles)
            {
                continue;
            }

            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            // Get position accessor (required)
            auto posIt = primitive.findAttribute("POSITION");
            if (posIt == primitive.attributes.end())
            {
                continue;
            }

            const fastgltf::Accessor& posAccessor = asset.accessors[posIt->accessorIndex];
            size_t vertexCount = posAccessor.count;
            vertices.resize(vertexCount);

            // Load positions and update bounds
            fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, posAccessor,
                [&](glm::vec3 pos, size_t idx) {
                    vertices[idx].position = pos;
                    outModel.boundsMin = glm::min(outModel.boundsMin, pos);
                    outModel.boundsMax = glm::max(outModel.boundsMax, pos);
                });

            // Load normals
            auto normIt = primitive.findAttribute("NORMAL");
            if (normIt != primitive.attributes.end())
            {
                const fastgltf::Accessor& normAccessor = asset.accessors[normIt->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, normAccessor,
                    [&](glm::vec3 norm, size_t idx) {
                        vertices[idx].normal = norm;
                    });
            }
            else
            {
                for (auto& v : vertices)
                {
                    v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }
            }

            // Load texture coordinates
            auto texIt = primitive.findAttribute("TEXCOORD_0");
            if (texIt != primitive.attributes.end())
            {
                const fastgltf::Accessor& texAccessor = asset.accessors[texIt->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, texAccessor,
                    [&](glm::vec2 uv, size_t idx) {
                        vertices[idx].texCoord = uv;
                    });
            }
            else
            {
                for (auto& v : vertices)
                {
                    v.texCoord = glm::vec2(0.0f);
                }
            }

            // Load vertex colors if present
            auto colorIt = primitive.findAttribute("COLOR_0");
            if (colorIt != primitive.attributes.end())
            {
                const fastgltf::Accessor& colorAccessor = asset.accessors[colorIt->accessorIndex];

                if (colorAccessor.type == fastgltf::AccessorType::Vec4)
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, colorAccessor,
                        [&](glm::vec4 color, size_t idx) {
                            vertices[idx].color = color;
                        });
                }
                else if (colorAccessor.type == fastgltf::AccessorType::Vec3)
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, colorAccessor,
                        [&](glm::vec3 color, size_t idx) {
                            vertices[idx].color = glm::vec4(color, 1.0f);
                        });
                }
            }
            else
            {
                for (auto& v : vertices)
                {
                    v.color = glm::vec4(1.0f);
                }
            }

            // Load indices
            if (primitive.indicesAccessor.has_value())
            {
                const fastgltf::Accessor& idxAccessor = asset.accessors[primitive.indicesAccessor.value()];
                indices.reserve(idxAccessor.count);

                fastgltf::iterateAccessor<uint32_t>(asset, idxAccessor,
                    [&](uint32_t index) {
                        indices.push_back(index);
                    });
            }

            // Create mesh
            auto mesh = std::make_unique<Mesh>();
            if (mesh->Initialize(device, vertices, indices))
            {
                // Assign material
                if (primitive.materialIndex.has_value())
                {
                    size_t matIdx = primitive.materialIndex.value();
                    if (matIdx < outModel.materials.size())
                    {
                        mesh->SetMaterial(outModel.materials[matIdx]);
                    }
                }
                else
                {
                    auto defaultMat = std::make_shared<Material>();
                    defaultMat->baseColorFactor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
                    defaultMat->metallicFactor = 0.0f;
                    defaultMat->roughnessFactor = 0.5f;
                    mesh->SetMaterial(defaultMat);
                }

                outModel.meshes.push_back(std::move(mesh));
            }
        }
    }

    // Calculate center and size for camera framing
    outModel.center = (outModel.boundsMin + outModel.boundsMax) * 0.5f;
    outModel.size = glm::length(outModel.boundsMax - outModel.boundsMin);

    sprintf_s(msg, "Model bounds: min(%.2f, %.2f, %.2f) max(%.2f, %.2f, %.2f) size: %.2f\n",
        outModel.boundsMin.x, outModel.boundsMin.y, outModel.boundsMin.z,
        outModel.boundsMax.x, outModel.boundsMax.y, outModel.boundsMax.z,
        outModel.size);
    OutputDebugStringA(msg);

    return !outModel.meshes.empty();
}

bool ModelLoader::LoadGLTF(const char* filepath, RenderDevice* device,
    std::vector<std::unique_ptr<Mesh>>& outMeshes)
{
    LoadedModel model;
    if (!LoadGLTF(filepath, device, model))
    {
        return false;
    }

    outMeshes = std::move(model.meshes);
    return true;
}
