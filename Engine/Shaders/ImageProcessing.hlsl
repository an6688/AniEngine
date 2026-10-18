// Input is the rendered RGBA8 display signal, not linear radiometric luminance.
Texture2D<float4> SceneColor : register(t0);
cbuffer Settings : register(b0) {
    uint Width; uint Height; uint Mode; uint Compare;
    float Strength; float Split;
};
float4 VSMain(uint vertex : SV_VertexID) : SV_Position {
    // Oversized fullscreen triangle; no vertex buffer or interpolated UV needed.
    float2 position = float2((vertex << 1) & 2, vertex & 2);
    return float4(position * float2(2, -2) + float2(-1, 1), 0, 1);
}
float Brightness(int2 p) {
    p = clamp(p, int2(0, 0), int2(Width - 1, Height - 1));
    return dot(SceneColor.Load(int3(p, 0)).rgb, float3(0.2126, 0.7152, 0.0722));
}
float4 PSMain(float4 position : SV_Position) : SV_Target {
    int2 p = int2(position.xy);
    float4 original = SceneColor.Load(int3(p, 0));
    if (Mode == 0 || (Compare != 0 && position.x < Split * Width)) return original;
    float value = Brightness(p);
    if (Mode == 2) {
        float tl = Brightness(p + int2(-1, -1)), tc = Brightness(p + int2(0, -1));
        float tr = Brightness(p + int2(1, -1)), ml = Brightness(p + int2(-1, 0));
        float mr = Brightness(p + int2(1, 0)), bl = Brightness(p + int2(-1, 1));
        float bc = Brightness(p + int2(0, 1)), br = Brightness(p + int2(1, 1));
        float gx = -tl + tr - 2 * ml + 2 * mr - bl + br;
        float gy = -tl - 2 * tc - tr + bl + 2 * bc + br;
        // A full black-to-white axis-aligned step has magnitude four.
        value = saturate(length(float2(gx, gy)) * 0.25 * Strength);
    }
    return float4(value.xxx, original.a);
}
