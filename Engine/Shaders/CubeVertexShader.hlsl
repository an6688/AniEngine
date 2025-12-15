// Cube vertex shader - pass normal to pixel shader

cbuffer TransformBuffer : register(b0)
{
    float4x4 worldViewProjection;
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
    float3 normal : NORMAL; // Pass normal through
    float2 texCoord : TEXCOORD; // Pass texcoord through
    float4 color : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    // Transform position by world-view-projection matrix
    output.position = mul(float4(input.position, 1.0f), worldViewProjection);
    
    // Pass through normal (for lighting)
    output.normal = input.normal;
    
    // Pass through texcoord (for future texture mapping)
    output.texCoord = input.texCoord;
    
    // Pass through color
    output.color = input.color;
    
    return output;
}