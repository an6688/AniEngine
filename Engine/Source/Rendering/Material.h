#pragma once

#include <memory>
#include <string>
#include <cstdint>
#include <glm/glm.hpp>

class Texture;

// PBR Material following glTF 2.0 spec
struct Material
{
    std::string name;

    // Base color / albedo
    std::shared_ptr<Texture> baseColorTexture;
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    int baseColorTexCoord = 0;

    // Metallic-Roughness
    std::shared_ptr<Texture> metallicRoughnessTexture;
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    int metallicRoughnessTexCoord = 0;

    // Normal map
    std::shared_ptr<Texture> normalTexture;
    float normalScale = 1.0f;
    int normalTexCoord = 0;

    // Occlusion
    std::shared_ptr<Texture> occlusionTexture;
    float occlusionStrength = 1.0f;
    int occlusionTexCoord = 0;

    // Emissive
    std::shared_ptr<Texture> emissiveTexture;
    glm::vec3 emissiveFactor = glm::vec3(0.0f);
    int emissiveTexCoord = 0;

    // Alpha mode
    enum class AlphaMode
    {
        Opaque,
        Mask,
        Blend
    };

    AlphaMode alphaMode = AlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;

    bool HasTextures() const
    {
        return baseColorTexture || metallicRoughnessTexture ||
            normalTexture || occlusionTexture || emissiveTexture;
    }
};