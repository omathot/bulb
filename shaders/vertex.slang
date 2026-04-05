struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inUV;
    float3 inNormal;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

[[vk::binding(0, 1)]]
ConstantBuffer<UniformBuffer> ubo;

struct VSOutput {
    float4 pos : SV_Position;
    float3 color;
    float2 uv;
    float3 worldNormal;
    float3 worldPos;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    float4 world_pos = mul(ubo.model, float4(input.inPosition, 1.0));
    output.pos = mul(ubo.proj, mul(ubo.view, world_pos));
    output.color = input.inColor;
    output.uv = input.inUV;
    output.worldPos = world_pos.xyz;
    output.worldNormal = mul((float3x3)ubo.model, input.inNormal);
    return output;
}
