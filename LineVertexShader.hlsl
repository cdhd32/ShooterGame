cbuffer SceneData : register(b0)
{
    matrix view;
    matrix proj;
    float3 eye;
};

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

VSOutput LineVS(VSInput input)
{
    VSOutput output;

    output.position = mul(mul(proj, view), float4(input.position, 1.0f));

    output.color = input.color;

    return output;
}