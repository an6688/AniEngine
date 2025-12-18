// TexturedVS.hlsl - Vertex shader for textured meshes

cbuffer TransformBuffer : register(b0)
{
    float4x4 worldViewProjection;
    float4x4 world;
    float3 cameraPosition;
    float padding;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 texCoord : TEXCOORD2;
    float4 color : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    // Transform position
    output.position = mul(float4(input.position, 1.0f), worldViewProjection);
    
    // World position for lighting
    output.worldPos = mul(float4(input.position, 1.0f), world).xyz;
    
    // Transform normal to world space (simplified - assumes uniform scale)
    output.normal = normalize(mul(float4(input.normal, 0.0f), world).xyz);
    
    // Pass through texcoord and color
    output.texCoord = input.texCoord;
    output.color = input.color;
    
    return output;
}
