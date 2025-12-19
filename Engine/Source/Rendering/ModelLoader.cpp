#include "ModelLoader.h"
#include "Mesh.h"
#include "Texture.h"
#include "TextureManager.h"
#include "Material.h"
#include "RenderDevice.h"
#include "VertexFormats.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <filesystem>
#include <fstream>
#include <stack>

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

static std::shared_ptr<Texture> GetDefaultTexture(TextureManager* mgr, bool sRGB)
{
    if (mgr) {
        return mgr->GetDefaultWhiteTexture();
    }
    return nullptr;
}

static std::shared_ptr<Texture> LoadTextureFromGltf(
    const fastgltf::Asset& asset,
    size_t textureIndex,
    const std::string& basePath,
    RenderDevice* device,
    TextureManager* textureManager,
    bool sRGB,
    std::unordered_map<size_t, std::shared_ptr<Texture>>& textureCache)
{
    auto cacheIt = textureCache.find(textureIndex);
    if (cacheIt != textureCache.end()) { 
        return cacheIt->second; 
    }

    if (textureIndex >= asset.textures.size()) {
        return GetDefaultTexture(textureManager, sRGB);
    }   

    const fastgltf::Texture& gltfTexture = asset.textures[textureIndex];

    size_t imageIndex;
    if (gltfTexture.imageIndex.has_value())
        imageIndex = gltfTexture.imageIndex.value();
    else
        return GetDefaultTexture(textureManager, sRGB);

    if (imageIndex >= asset.images.size())
        return GetDefaultTexture(textureManager, sRGB);

    const fastgltf::Image& image = asset.images[imageIndex];
    std::shared_ptr<Texture> texture;

    std::visit(fastgltf::visitor{
        [](auto& arg) {},
        [&](const fastgltf::sources::URI& uri) {
            std::string imagePath = basePath + "/" + std::string(uri.uri.path());
            if (textureManager)
                texture = textureManager->LoadTexture(imagePath, sRGB);
            else {
                texture = std::make_shared<Texture>();
                if (!texture->LoadFromFile(device, imagePath, sRGB))
                    texture = nullptr;
            }
        },
        [&](const fastgltf::sources::Array& array) {
            if (textureManager)
                texture = textureManager->LoadTextureFromMemory(
                    image.name.empty() ? "embedded" : std::string(image.name),
                    reinterpret_cast<const uint8_t*>(array.bytes.data()),
                    array.bytes.size(), sRGB);
            else {
                texture = std::make_shared<Texture>();
                if (!texture->LoadFromMemory(device,
                    reinterpret_cast<const uint8_t*>(array.bytes.data()),
                    array.bytes.size(), sRGB))
                    texture = nullptr;
            }
        },
        [&](const fastgltf::sources::Vector& vector) {
            if (textureManager)
                texture = textureManager->LoadTextureFromMemory(
                    image.name.empty() ? "embedded" : std::string(image.name),
                    reinterpret_cast<const uint8_t*>(vector.bytes.data()),
                    vector.bytes.size(), sRGB);
            else {
                texture = std::make_shared<Texture>();
                if (!texture->LoadFromMemory(device,
                    reinterpret_cast<const uint8_t*>(vector.bytes.data()),
                    vector.bytes.size(), sRGB))
                    texture = nullptr;
            }
        },
        [&](const fastgltf::sources::BufferView& bufferViewSource) {
            const fastgltf::BufferView& bufferView = asset.bufferViews[bufferViewSource.bufferViewIndex];
            const fastgltf::Buffer& buffer = asset.buffers[bufferView.bufferIndex];

            std::visit(fastgltf::visitor{
                [](auto& arg) {},
                [&](const fastgltf::sources::Array& array) {
                    const uint8_t* data = reinterpret_cast<const uint8_t*>(array.bytes.data()) + bufferView.byteOffset;
                    if (textureManager)
                        texture = textureManager->LoadTextureFromMemory(
                            image.name.empty() ? "embedded" : std::string(image.name),
                            data, bufferView.byteLength, sRGB);
                    else {
                        texture = std::make_shared<Texture>();
                        if (!texture->LoadFromMemory(device, data, bufferView.byteLength, sRGB))
                            texture = nullptr;
                    }
                },
                [&](const fastgltf::sources::Vector& vector) {
                    const uint8_t* data = reinterpret_cast<const uint8_t*>(vector.bytes.data()) + bufferView.byteOffset;
                    if (textureManager)
                        texture = textureManager->LoadTextureFromMemory(
                            image.name.empty() ? "embedded" : std::string(image.name),
                            data, bufferView.byteLength, sRGB);
                    else {
                        texture = std::make_shared<Texture>();
                        if (!texture->LoadFromMemory(device, data, bufferView.byteLength, sRGB))
                            texture = nullptr;
                    }
                }
            }, buffer.data);
        }
        }, image.data);

	if (!texture) {
		texture = GetDefaultTexture(textureManager, sRGB);
	}
	if (texture) {
		textureCache[textureIndex] = texture;
	}
	return texture;
}

// Compute local transform matrix from TRS or matrix
static glm::mat4 ComputeLocalTransform(const fastgltf::Node& node)
{
    glm::mat4 localTransform(1.0f);

    if (auto* trs = std::get_if<fastgltf::TRS>(&node.transform))
    {
        glm::vec3 translation(trs->translation[0], trs->translation[1], trs->translation[2]);
        glm::quat rotation(trs->rotation[3], trs->rotation[0], trs->rotation[1], trs->rotation[2]);
        glm::vec3 scale(trs->scale[0], trs->scale[1], trs->scale[2]);

        glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
        glm::mat4 R = glm::toMat4(rotation);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);

        localTransform = T * R * S;
    }
    else if (auto* mat = std::get_if<fastgltf::math::fmat4x4>(&node.transform))
    {
        // fastgltf stores matrices in column-major order (same as glm)
        for (int col = 0; col < 4; col++) {
            for (int row = 0; row < 4; row++) {
                localTransform[col][row] = (*mat)[col][row];
            }
        }       
    }

    return localTransform;
}

// Recursive function to process nodes
static void ProcessNodeHierarchy(
    const fastgltf::Asset& asset,
    size_t nodeIdx,
    const glm::mat4& parentWorldTransform,
    std::vector<glm::mat4>& nodeWorldTransforms,
    int depth = 0)
{
    const auto& node = asset.nodes[nodeIdx];

    // Compute this node's LOCAL transform
    glm::mat4 localTransform = ComputeLocalTransform(node);

    // WORLD transform = parent's world * this node's local
    glm::mat4 worldTransform = parentWorldTransform * localTransform;
    nodeWorldTransforms[nodeIdx] = worldTransform;

    // Debug output with indentation showing hierarchy
    char msg[512];
    std::string indent(depth * 2, ' ');
    glm::vec3 localPos(localTransform[3]);
    glm::vec3 worldPos(worldTransform[3]);

    // Check if there's a rotation by looking at the diagonal
    bool hasRotation = (localTransform[0][0] != 1.0f || localTransform[1][1] != 1.0f || localTransform[2][2] != 1.0f);

    if (hasRotation && !node.meshIndex.has_value())
    {
        sprintf_s(msg, "%sNode %zu '%s': local(%.2f, %.2f, %.2f) world(%.2f, %.2f, %.2f) mesh=%s [HAS ROTATION]\n",
            indent.c_str(), nodeIdx, node.name.c_str(),
            localPos.x, localPos.y, localPos.z,
            worldPos.x, worldPos.y, worldPos.z,
            "none");
    }
    else
    {
        sprintf_s(msg, "%sNode %zu '%s': local(%.2f, %.2f, %.2f) world(%.2f, %.2f, %.2f) mesh=%s\n",
            indent.c_str(), nodeIdx, node.name.c_str(),
            localPos.x, localPos.y, localPos.z,
            worldPos.x, worldPos.y, worldPos.z,
            node.meshIndex.has_value() ? std::to_string(node.meshIndex.value()).c_str() : "none");
    }
    OutputDebugStringA(msg);

    // Recursively process children with THIS node's world transform as their parent
    for (size_t childIdx : node.children)
    {
        ProcessNodeHierarchy(asset, childIdx, worldTransform, nodeWorldTransforms, depth + 1);
    }
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

    constexpr auto gltfOptions =
        fastgltf::Options::LoadExternalBuffers |
        fastgltf::Options::LoadExternalImages |
        fastgltf::Options::GenerateMeshIndices;

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
        assetResult = parser.loadGltf(dataBuffer, path.parent_path(), gltfOptions);
    else if (type == fastgltf::GltfType::GLB)
        assetResult = parser.loadGltfBinary(dataBuffer, path.parent_path(), gltfOptions);
    else
    {
        OutputDebugStringA("Unknown glTF file type\n");
        return false;
    }

    if (assetResult.error() != fastgltf::Error::None)
    {
        char errorMsg[512];
        sprintf_s(errorMsg, "Failed to parse glTF: %s (code %d)\n",
            GetFastGltfErrorString(assetResult.error()),
            static_cast<int>(assetResult.error()));
        OutputDebugStringA(errorMsg);
        return false;
    }

    fastgltf::Asset& asset = assetResult.get();
    std::string basePath = path.parent_path().string();

    char msg[512];
    sprintf_s(msg, "glTF has %zu meshes, %zu materials, %zu textures, %zu nodes, %zu scenes\n",
        asset.meshes.size(), asset.materials.size(),
        asset.textures.size(), asset.nodes.size(), asset.scenes.size());
    OutputDebugStringA(msg);

    // Initialize bounds
    outModel.boundsMin = glm::vec3(FLT_MAX);
    outModel.boundsMax = glm::vec3(-FLT_MAX);

    std::unordered_map<size_t, std::shared_ptr<Texture>> textureCache;

    // Load materials
    outModel.materials.reserve(asset.materials.size());

    for (size_t matIdx = 0; matIdx < asset.materials.size(); matIdx++)
    {
        const auto& gltfMat = asset.materials[matIdx];
        auto material = std::make_shared<Material>();
        material->name = std::string(gltfMat.name);

        const auto& pbr = gltfMat.pbrData;

        material->baseColorFactor = glm::vec4(
            pbr.baseColorFactor[0], pbr.baseColorFactor[1],
            pbr.baseColorFactor[2], pbr.baseColorFactor[3]);

        if (pbr.baseColorTexture.has_value())
        {
            material->baseColorTexture = LoadTextureFromGltf(
                asset, pbr.baseColorTexture->textureIndex,
                basePath, device, m_textureManager, true, textureCache);

            sprintf_s(msg, "  Material %zu '%s': baseColor texture index %zu\n",
                matIdx, gltfMat.name.c_str(), pbr.baseColorTexture->textureIndex);
            OutputDebugStringA(msg);
        }

        material->metallicFactor = pbr.metallicFactor;
        material->roughnessFactor = pbr.roughnessFactor;

        if (pbr.metallicRoughnessTexture.has_value())
            material->metallicRoughnessTexture = LoadTextureFromGltf(
                asset, pbr.metallicRoughnessTexture->textureIndex,
                basePath, device, m_textureManager, false, textureCache);

        if (gltfMat.normalTexture.has_value())
        {
            material->normalTexture = LoadTextureFromGltf(
                asset, gltfMat.normalTexture->textureIndex,
                basePath, device, m_textureManager, false, textureCache);
            material->normalScale = gltfMat.normalTexture->scale;
        }

        if (gltfMat.occlusionTexture.has_value())
        {
            material->occlusionTexture = LoadTextureFromGltf(
                asset, gltfMat.occlusionTexture->textureIndex,
                basePath, device, m_textureManager, false, textureCache);
            material->occlusionStrength = gltfMat.occlusionTexture->strength;
        }

        material->emissiveFactor = glm::vec3(
            gltfMat.emissiveFactor[0], gltfMat.emissiveFactor[1], gltfMat.emissiveFactor[2]);

        if (gltfMat.emissiveTexture.has_value())
            material->emissiveTexture = LoadTextureFromGltf(
                asset, gltfMat.emissiveTexture->textureIndex,
                basePath, device, m_textureManager, true, textureCache);

        if (gltfMat.alphaMode == fastgltf::AlphaMode::Mask)
        {
            material->alphaMode = Material::AlphaMode::Mask;
            material->alphaCutoff = gltfMat.alphaCutoff;
        }
        else if (gltfMat.alphaMode == fastgltf::AlphaMode::Blend)
            material->alphaMode = Material::AlphaMode::Blend;
        else
            material->alphaMode = Material::AlphaMode::Opaque;

        material->doubleSided = gltfMat.doubleSided;
        outModel.materials.push_back(material);
    }

    for (auto& pair : textureCache)
        outModel.textures.push_back(pair.second);

    // Load mesh primitives
    std::unordered_map<size_t, std::vector<std::shared_ptr<Mesh>>> meshPrimitives;

    for (size_t meshIdx = 0; meshIdx < asset.meshes.size(); meshIdx++)
    {
        const auto& gltfMesh = asset.meshes[meshIdx];
        std::vector<std::shared_ptr<Mesh>> primitives;

        sprintf_s(msg, "Loading mesh %zu '%s' with %zu primitives\n",
            meshIdx, gltfMesh.name.c_str(), gltfMesh.primitives.size());
        OutputDebugStringA(msg);

        for (size_t primIdx = 0; primIdx < gltfMesh.primitives.size(); primIdx++)
        {
            const auto& primitive = gltfMesh.primitives[primIdx];

            if (primitive.type != fastgltf::PrimitiveType::Triangles)
                continue;

            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            auto posIt = primitive.findAttribute("POSITION");
            if (posIt == primitive.attributes.end())
                continue;

            const fastgltf::Accessor& posAccessor = asset.accessors[posIt->accessorIndex];
            size_t vertexCount = posAccessor.count;
            vertices.resize(vertexCount);

            // Track local mesh bounds
            glm::vec3 meshMin(FLT_MAX), meshMax(-FLT_MAX);

            fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, posAccessor,
                [&](glm::vec3 pos, size_t idx) {
                    vertices[idx].position = pos;
                    meshMin = glm::min(meshMin, pos);
                    meshMax = glm::max(meshMax, pos);
                });

            sprintf_s(msg, "  Primitive %zu: %zu verts, local bounds (%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f)\n",
                primIdx, vertexCount, meshMin.x, meshMin.y, meshMin.z, meshMax.x, meshMax.y, meshMax.z);
            OutputDebugStringA(msg);

            auto normIt = primitive.findAttribute("NORMAL");
            if (normIt != primitive.attributes.end())
            {
                const fastgltf::Accessor& normAccessor = asset.accessors[normIt->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, normAccessor,
                    [&](glm::vec3 norm, size_t idx) { vertices[idx].normal = norm; });
            }
            else
                for (auto& v : vertices) v.normal = glm::vec3(0.0f, 1.0f, 0.0f);

            auto texIt = primitive.findAttribute("TEXCOORD_0");
            if (texIt != primitive.attributes.end())
            {
                const fastgltf::Accessor& texAccessor = asset.accessors[texIt->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, texAccessor,
                    [&](glm::vec2 uv, size_t idx) { vertices[idx].texCoord = uv; });
            }
            else
                for (auto& v : vertices) v.texCoord = glm::vec2(0.0f);

            auto colorIt = primitive.findAttribute("COLOR_0");
            if (colorIt != primitive.attributes.end())
            {
                const fastgltf::Accessor& colorAccessor = asset.accessors[colorIt->accessorIndex];
                if (colorAccessor.type == fastgltf::AccessorType::Vec4)
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, colorAccessor,
                        [&](glm::vec4 color, size_t idx) { vertices[idx].color = color; });
                else if (colorAccessor.type == fastgltf::AccessorType::Vec3)
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, colorAccessor,
                        [&](glm::vec3 color, size_t idx) { vertices[idx].color = glm::vec4(color, 1.0f); });
            }
            else
                for (auto& v : vertices) v.color = glm::vec4(1.0f);

            if (primitive.indicesAccessor.has_value())
            {
                const fastgltf::Accessor& idxAccessor = asset.accessors[primitive.indicesAccessor.value()];
                indices.reserve(idxAccessor.count);
                fastgltf::iterateAccessor<uint32_t>(asset, idxAccessor,
                    [&](uint32_t index) { indices.push_back(index); });
            }

            auto mesh = std::make_shared<Mesh>();
            if (mesh->Initialize(device, vertices, indices))
            {
                // Store local bounds in mesh for later world space calculation
                mesh->SetLocalBounds(meshMin, meshMax);

                if (primitive.materialIndex.has_value())
                {
                    size_t matIdx = primitive.materialIndex.value();
                    if (matIdx < outModel.materials.size())
                        mesh->SetMaterial(outModel.materials[matIdx]);
                }
                else
                {
                    auto defaultMat = std::make_shared<Material>();
                    defaultMat->baseColorFactor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
                    defaultMat->metallicFactor = 0.0f;
                    defaultMat->roughnessFactor = 0.5f;
                    mesh->SetMaterial(defaultMat);
                }

                primitives.push_back(mesh);
                outModel.meshes.push_back(mesh);
            }
        }

        meshPrimitives[meshIdx] = primitives;
    }

    // Compute world transforms for all nodes
    std::vector<glm::mat4> nodeWorldTransforms(asset.nodes.size(), glm::mat4(1.0f));

    OutputDebugStringA("\nProcessing node hierarchy:\n");

    size_t sceneIndex = asset.defaultScene.has_value() ? asset.defaultScene.value() : 0;

    if (sceneIndex < asset.scenes.size())
    {
        const auto& scene = asset.scenes[sceneIndex];
        sprintf_s(msg, "Scene '%s' has %zu root nodes\n",
            scene.name.c_str(), scene.nodeIndices.size());
        OutputDebugStringA(msg);

        // Process each root node with identity as parent transform
        for (size_t rootNodeIdx : scene.nodeIndices)
        {
            ProcessNodeHierarchy(asset, rootNodeIdx, glm::mat4(1.0f), nodeWorldTransforms, 0);
        }
    }

    // Create mesh instances and compute world bounds
    OutputDebugStringA("\nCreating mesh instances:\n");

    for (size_t nodeIdx = 0; nodeIdx < asset.nodes.size(); nodeIdx++)
    {
        const auto& node = asset.nodes[nodeIdx];

        if (!node.meshIndex.has_value()) {
            continue;
        }   

        size_t meshIdx = node.meshIndex.value();
        auto it = meshPrimitives.find(meshIdx);
        if (it == meshPrimitives.end() || it->second.empty())
            continue;

        const glm::mat4& worldTransform = nodeWorldTransforms[nodeIdx];
        glm::vec3 worldPos(worldTransform[3]);

        for (const auto& mesh : it->second)
        {
            MeshInstance instance;
            instance.mesh = mesh;
            instance.transform = worldTransform;
            outModel.meshInstances.push_back(instance);

            // Transform mesh local bounds to world space
            glm::vec3 localMin, localMax;
            mesh->GetLocalBounds(localMin, localMax);

            // Transform all 8 corners of the local AABB
            glm::vec3 corners[8] = {
                glm::vec3(localMin.x, localMin.y, localMin.z),
                glm::vec3(localMax.x, localMin.y, localMin.z),
                glm::vec3(localMin.x, localMax.y, localMin.z),
                glm::vec3(localMax.x, localMax.y, localMin.z),
                glm::vec3(localMin.x, localMin.y, localMax.z),
                glm::vec3(localMax.x, localMin.y, localMax.z),
                glm::vec3(localMin.x, localMax.y, localMax.z),
                glm::vec3(localMax.x, localMax.y, localMax.z)
            };

            for (int i = 0; i < 8; i++)
            {
                glm::vec4 worldCorner = worldTransform * glm::vec4(corners[i], 1.0f);
                outModel.boundsMin = glm::min(outModel.boundsMin, glm::vec3(worldCorner));
                outModel.boundsMax = glm::max(outModel.boundsMax, glm::vec3(worldCorner));
            }
        }

        sprintf_s(msg, "  Node %zu '%s' -> mesh %zu: %zu instances at world(%.2f, %.2f, %.2f)\n",
            nodeIdx, node.name.c_str(), meshIdx, it->second.size(),
            worldPos.x, worldPos.y, worldPos.z);
        OutputDebugStringA(msg);
    }

    // Calculate center and size
    outModel.center = (outModel.boundsMin + outModel.boundsMax) * 0.5f;
    outModel.size = glm::length(outModel.boundsMax - outModel.boundsMin);

    sprintf_s(msg, "\nFinal: %zu instances, world bounds (%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f) size: %.2f\n",
        outModel.meshInstances.size(),
        outModel.boundsMin.x, outModel.boundsMin.y, outModel.boundsMin.z,
        outModel.boundsMax.x, outModel.boundsMax.y, outModel.boundsMax.z,
        outModel.size);
    OutputDebugStringA(msg);

    return !outModel.meshInstances.empty();
}

bool ModelLoader::LoadGLTF(const char* filepath, RenderDevice* device,
    std::vector<std::unique_ptr<Mesh>>& outMeshes)
{
    LoadedModel model;
    if (!LoadGLTF(filepath, device, model)) {
        return false;
    }   

    for (auto& mesh : model.meshes) {
        outMeshes.push_back(std::unique_ptr<Mesh>(new Mesh(*mesh)));
    }   
    return true;
}