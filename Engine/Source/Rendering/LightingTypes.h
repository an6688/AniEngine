#pragma once

#include <glm/glm.hpp>

static const int MAX_LIGHTS = 8;

// GPU Light structure - must match HLSL Light struct exactly (64 bytes)
struct GPULight {
    glm::vec3 position;
    float type;              // 0 = directional, 1 = point, 2 = spot

    glm::vec3 direction;
    float intensity;

    glm::vec3 color;
    float range;

    float innerConeAngle;    // Cosine of angle (for spot)
    float outerConeAngle;    // Cosine of angle (for spot)
    float enabled;
    float padding;
};

// Full lighting constant buffer - must match HLSL LightingConstants
struct LightingConstantBuffer {
    GPULight lights[MAX_LIGHTS];  // 64 * 8 = 512 bytes
    glm::vec3 ambientColor;
    float numActiveLights;        // 16 bytes total for this row
    // Total: 528 bytes
};

// Static assert to verify size matches expectations
static_assert(sizeof(GPULight) == 64, "GPULight must be 64 bytes");
static_assert(sizeof(LightingConstantBuffer) == 528, "LightingConstantBuffer must be 528 bytes");
