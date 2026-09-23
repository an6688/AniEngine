cbuffer ShadowDraw : register(b0)
{
    float4x4 lightWorldViewProjection;
    float4 alphaSettings; // factor, cutoff, use texture, unused
};
Texture2D baseColor : register(t0);
SamplerState materialSampler : register(s0);
struct Input {
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};
struct Output {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float alpha : COLOR;
};
Output VSMain(Input input) {
    Output result;
    result.position = mul(float4(input.position, 1.0f), lightWorldViewProjection);
    result.uv = input.uv;
    result.alpha = input.color.a;
    return result;
}
void PSMain(Output input) {
    float alpha = alphaSettings.x * input.alpha;
    if (alphaSettings.z > 0.5f) alpha *= baseColor.Sample(materialSampler, input.uv).a;
    clip(alpha - alphaSettings.y);
}
