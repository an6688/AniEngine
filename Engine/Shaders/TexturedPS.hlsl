// TexturedPS.hlsl - Pixel shader with texture sampling

// Texture and sampler
Texture2D baseColorTexture : register(t0);
SamplerState linearSampler : register(s0);

// Material properties (optional, for when you don't have a texture)
cbuffer MaterialBuffer : register(b1)
{
    float4 baseColorFactor;
    float hasBaseColorTexture;
    float3 padding;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 texCoord : TEXCOORD2;
    float4 color : COLOR;
};

float4 main(PSInput input) : SV_TARGET
{
    // Sample base color texture
    float4 baseColor;
    if (hasBaseColorTexture > 0.5f)
    {
        baseColor = baseColorTexture.Sample(linearSampler, input.texCoord);
    }
    else
    {
        baseColor = baseColorFactor;
    }
    
    // Multiply by vertex color
    baseColor *= input.color;
    
    // Simple directional lighting
    float3 lightDir = normalize(float3(0.5f, -1.0f, 0.5f));
    float3 normal = normalize(input.normal);
    
    float diffuse = max(dot(-lightDir, normal), 0.0f);
    float3 ambient = float3(0.3f, 0.3f, 0.3f);
    float3 lit = ambient + diffuse * 0.7f;
    
    return float4(baseColor.rgb * lit, baseColor.a);
}
