[[vk::binding(0, 2)]]
Texture2D<float4> texture;
[[vk::binding(0, 2)]]
SamplerState sampler;

struct VSOutput {
    float4 pos : SV_Position;
    float3 color;
    float2 uv;
};

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return texture.Sample(sampler, vertIn.uv);
}
