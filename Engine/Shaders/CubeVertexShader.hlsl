// Cube vertex shader uses matrices for 3D transforms

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
    float4 color : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    // Transform position by world view projection matrix
    output.position = mul(float4(input.position, 1.0f), worldViewProjection);
    
    // Pass through color
    output.color = input.color;
    
    return output;
}