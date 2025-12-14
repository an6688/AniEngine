// Pixel shader. Colors the pixels

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

float4 main(PSInput input) : SV_TARGET
{
    // Return the interpolated color
    return input.color;
}