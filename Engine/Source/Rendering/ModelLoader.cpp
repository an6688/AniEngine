#include "ModelLoader.h"
#include "RenderDevice.h"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <filesystem>

ModelLoader::ModelLoader()
{
}

ModelLoader::~ModelLoader()
{
}

bool ModelLoader::LoadGLTF(
    const std::string& filepath,
    RenderDevice* device,
    std::vector<std::unique_ptr<Mesh>>& outMeshes)
{
    if (!device) {
        return false;
    }   

    // Check if file exists
    if (!std::filesystem::exists(filepath)) {
        return false;
    }
        
    // Create FastGLTF parser
    fastgltf::Parser parser;

    // Load GLTF file
    auto gltfFile = fastgltf::MappedGltfFile::FromPath(filepath);
    if (!gltfFile) {
        return false;
    }
        
    // Parse GLTF
    auto asset = parser.loadGltf(
        gltfFile.get(),
        std::filesystem::path(filepath).parent_path(),
        fastgltf::Options::LoadExternalBuffers
    );

    if (asset.error() != fastgltf::Error::None) {
        return false;
    }   

    auto& gltf = asset.get();

    // Process all meshes in the GLTF file
    for (size_t meshIndex = 0; meshIndex < gltf.meshes.size(); ++meshIndex)
    {
        const auto& mesh = gltf.meshes[meshIndex];

        // Each mesh can have multiple primitives
        for (size_t primIndex = 0; primIndex < mesh.primitives.size(); ++primIndex)
        {
            const auto& primitive = mesh.primitives[primIndex];

            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            // Get position data (required)
            auto posAttr = primitive.findAttribute("POSITION");
            if (posAttr == primitive.attributes.end())
                continue;  // Skip primitives without positions

            const auto& posAccessor = gltf.accessors[posAttr->accessorIndex];

            // Reserve space
            vertices.resize(posAccessor.count);

            // Load positions - use fastgltf's native vec3 type
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, posAccessor,
                [&](fastgltf::math::fvec3 pos, size_t idx) {
                    vertices[idx].position = glm::vec3(pos.x(), pos.y(), pos.z());
                });

            // Load normals (if available)
            auto normalAttr = primitive.findAttribute("NORMAL");
            if (normalAttr != primitive.attributes.end())
            {
                const auto& normalAccessor = gltf.accessors[normalAttr->accessorIndex];
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, normalAccessor,
                    [&](fastgltf::math::fvec3 normal, size_t idx) {
                        vertices[idx].normal = glm::vec3(normal.x(), normal.y(), normal.z());
                    });
            }
            else
            {
                // Default normal (pointing up)
                for (auto& v : vertices)
                    v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            // Load texture coordinates (if available)
            auto texCoordAttr = primitive.findAttribute("TEXCOORD_0");
            if (texCoordAttr != primitive.attributes.end())
            {
                const auto& texCoordAccessor = gltf.accessors[texCoordAttr->accessorIndex];
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, texCoordAccessor,
                    [&](fastgltf::math::fvec2 uv, size_t idx) {
                        vertices[idx].texCoord = glm::vec2(uv.x(), uv.y());
                    });
            }
            else
            {
                // Default UV
                for (auto& v : vertices)
                    v.texCoord = glm::vec2(0.0f, 0.0f);
            }

            // Load vertex colors (if available)
            auto colorAttr = primitive.findAttribute("COLOR_0");
            if (colorAttr != primitive.attributes.end())
            {
                const auto& colorAccessor = gltf.accessors[colorAttr->accessorIndex];
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, colorAccessor,
                    [&](fastgltf::math::fvec4 color, size_t idx) {
                        vertices[idx].color = glm::vec4(color.x(), color.y(), color.z(), color.w());
                    });
            }
            else
            {
                // Default color (white)
                for (auto& v : vertices)
                    v.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            }

            // Load indices
            if (primitive.indicesAccessor.has_value())
            {
                const auto& indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];
                indices.resize(indexAccessor.count);

                // FastGLTF handles different index types automatically
                fastgltf::iterateAccessorWithIndex<uint32_t>(gltf, indexAccessor,
                    [&](uint32_t index, size_t idx) {
                        indices[idx] = index;
                    });
            }
            else
            {
                // Generate indices (no index buffer in GLTF)
                indices.resize(vertices.size());
                for (size_t i = 0; i < vertices.size(); ++i)
                    indices[i] = static_cast<uint32_t>(i);
            }

            // Create mesh object
            auto newMesh = std::make_unique<Mesh>();
            if (newMesh->Initialize(device, vertices, indices))
            {
                outMeshes.push_back(std::move(newMesh));
            }
        }
    }

    return !outMeshes.empty();
}