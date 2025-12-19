// TexturedVS.hlsl - Simple vertex shader for cube fallback

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
    float3 worldPos : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 texCoord : TEXCOORD2;
    float4 color : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    output.position = mul(float4(input.position, 1.0f), worldViewProjection);
    output.worldPos = input.position; // Not transformed for simple cube
    output.normal = input.normal;
    output.texCoord = input.texCoord;
    output.color = input.color;
    
    return output;
}
