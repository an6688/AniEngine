// TexturedPS.hlsl - Simple pixel shader for cube fallback

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
    // Simple directional lighting
    float3 lightDir = normalize(float3(0.5f, -1.0f, 0.5f));
    float3 normal = normalize(input.normal);
    
    float diffuse = max(dot(-lightDir, normal), 0.0f);
    float3 ambient = float3(0.3f, 0.3f, 0.3f);
    float3 lit = ambient + diffuse * 0.7f;
    
    // Use vertex color directly
    return float4(input.color.rgb * lit, input.color.a);
}
