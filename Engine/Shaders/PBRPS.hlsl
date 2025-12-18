// PBRPS.hlsl - Physically Based Rendering Pixel Shader

// Constant Buffers

cbuffer TransformConstants : register(b0)
{
    float4x4 worldViewProjection;
    float4x4 world;
    float4x4 worldInverseTranspose;
    float3 cameraPosition;
    float transformPadding;
};

cbuffer MaterialConstants : register(b1)
{
    float4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    float3 emissiveFactor;
    float alphaCutoff;
    
    float hasBaseColorTexture;
    float hasMetallicRoughnessTexture;
    float hasNormalTexture;
    float hasOcclusionTexture;
    float hasEmissiveTexture;
    float3 materialPadding;
};

// Textures and Samplers

Texture2D baseColorTexture : register(t0);
Texture2D metallicRoughnessTexture : register(t1);
Texture2D normalTexture : register(t2);
Texture2D occlusionTexture : register(t3);
Texture2D emissiveTexture : register(t4);

SamplerState linearSampler : register(s0);

// Input Structure

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 texCoord : TEXCOORD2;
    float4 color : COLOR;
};

// Constants

static const float PI = 3.14159265359f;

// Simple directional light
static const float3 lightDirection = normalize(float3(0.5f, -1.0f, 0.5f));
static const float3 lightColor = float3(1.0f, 1.0f, 1.0f);
static const float lightIntensity = 2.0f;

// PBR Functions

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;
    
    return num / max(denom, 0.0001f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    
    return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// Normal Mapping Helper

float3 GetNormalFromMap(float3 worldNormal, float3 worldPos, float2 texCoord)
{
    float3 tangentNormal = normalTexture.Sample(linearSampler, texCoord).rgb;
    tangentNormal = tangentNormal * 2.0f - 1.0f;
    tangentNormal.xy *= normalScale;
    tangentNormal = normalize(tangentNormal);
    
    // Calculate tangent and bitangent from screen-space derivatives
    float3 Q1 = ddx(worldPos);
    float3 Q2 = ddy(worldPos);
    float2 st1 = ddx(texCoord);
    float2 st2 = ddy(texCoord);
    
    float3 N = normalize(worldNormal);
    float3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    float3 B = -normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);
    
    return normalize(mul(tangentNormal, TBN));
}

// Main Pixel Shader

float4 main(PSInput input) : SV_TARGET
{
    // Sample Base Color
    float4 baseColor = baseColorFactor;
    if (hasBaseColorTexture > 0.5f)
    {
        float4 texColor = baseColorTexture.Sample(linearSampler, input.texCoord);
        baseColor *= texColor;
    }
    baseColor *= input.color;
    
    // Alpha cutoff test
    if (baseColor.a < alphaCutoff)
    {
        discard;
    }
    
    // Sample Metallic-Roughness
    float metallic = metallicFactor;
    float roughness = roughnessFactor;
    
    if (hasMetallicRoughnessTexture > 0.5f)
    {
        float4 mrSample = metallicRoughnessTexture.Sample(linearSampler, input.texCoord);
        roughness *= mrSample.g;
        metallic *= mrSample.b;
    }
    
    roughness = max(roughness, 0.04f);
    
    // Get Normal
    float3 N = normalize(input.normal);
    
    if (hasNormalTexture > 0.5f)
    {
        N = GetNormalFromMap(input.normal, input.worldPos, input.texCoord);
    }
    
    // Sample Occlusion
    float ao = 1.0f;
    if (hasOcclusionTexture > 0.5f)
    {
        ao = occlusionTexture.Sample(linearSampler, input.texCoord).r;
        ao = lerp(1.0f, ao, occlusionStrength);
    }
    
    // PBR Lighting Calculation
    
    float3 V = normalize(cameraPosition - input.worldPos);
    float3 L = normalize(-lightDirection);
    float3 H = normalize(V + L);
    
    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, baseColor.rgb, metallic);
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;
    float3 specular = numerator / denominator;
    
    float3 kS = F;
    float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
    kD *= 1.0f - metallic;
    
    float NdotL = max(dot(N, L), 0.0f);
    float3 radiance = lightColor * lightIntensity;
    
    float3 Lo = (kD * baseColor.rgb / PI + specular) * radiance * NdotL;
    
    // Ambient + Emissive
    float3 ambient = float3(0.03f, 0.03f, 0.03f) * baseColor.rgb * ao;
    
    float3 color = ambient + Lo;
    
    float3 emissive = emissiveFactor;
    if (hasEmissiveTexture > 0.5f)
    {
        emissive *= emissiveTexture.Sample(linearSampler, input.texCoord).rgb;
    }
    color += emissive;
    
    // Tone Mapping and Gamma Correction
    
    color = color / (color + float3(1.0f, 1.0f, 1.0f));
    color = pow(color, float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));
    
    return float4(color, baseColor.a);
}
