// Vertex shader. Transforms vertices to screen space

struct VSInput
{
    float3 position : POSITION;
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
    
    // Pass through position. Already in clip space for this simple triangle
    output.position = float4(input.position, 1.0f);
    
    // Pass through color
    output.color = input.color;
    
    return output;
}