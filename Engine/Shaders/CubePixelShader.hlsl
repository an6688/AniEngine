// Cube pixel shader - with basic lighting

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float4 color : COLOR;
};

float4 main(PSInput input) : SV_TARGET
{
    // Simple directional light
    float3 lightDir = normalize(float3(0.5f, -1.0f, 0.5f)); // Light from above-right
    float3 normal = normalize(input.normal);
    
    // Diffuse lighting (Lambertian)
    float diffuse = max(dot(-lightDir, normal), 0.0f);
    
    // Ambient + diffuse
    float3 ambient = float3(0.3f, 0.3f, 0.3f);
    float3 lit = ambient + diffuse * 0.7f;
    
    // Apply lighting to vertex color
    return float4(input.color.rgb * lit, input.color.a);
}