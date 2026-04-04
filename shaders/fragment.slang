struct VSOutput {
    float4 pos : SV_Position;
    float3 color;
};

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return float4(vertIn.color, 1.0);
}
