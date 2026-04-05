[[vk::binding(0, 2)]]
Texture2D<float4> texture;
[[vk::binding(0, 2)]]
SamplerState sampler;


struct LightBuffer {
    float3 lightDir;
    float _pad1;
    float3 lightColor;
    float _pad2;
    float3 cameraPos;
    float enabled;
};
[[vk::binding(0, 3)]]
ConstantBuffer<LightBuffer> light;

struct VSOutput {
    float4 pos : SV_Position;
    float3 color;
    float2 uv;
    float3 worldNormal;
    float3 worldPos;
};

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    float3 texColor = texture.Sample(sampler, vertIn.uv).rgb;
    if (light.enabled < 0.5)
        return float4(texColor, 1.0);

    float3 N = normalize(vertIn.worldNormal);
    float3 L = normalize(-light.lightDir);

    float ambiant = 0.15;
    float diff = max(dot(N, L), 0.0);

    float3 V = normalize(light.cameraPos - vertIn.worldPos);
    float3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    float3 res = (ambiant + diff + spec * 0.5) * light.lightColor * texColor;
    return float4(res, 1.0);
}
