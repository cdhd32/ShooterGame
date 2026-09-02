struct Output
{
    float4 svpos : SV_POSITION; // 시스템용 정점 좌표
    float4 pos : POSITION; // 정점 좌표
    float4 normal : NORMAL0;
    float4 vnormal : NORMAL1;
    float2 uv  :TEXCOORD; // uv 값
    float3 ray : VECTOR; // 광선 방향
};

cbuffer SceneData : register(b0)
{
    matrix view;
    matrix proj;
    float3 eye;
};

cbuffer Transform : register(b1)
{
    matrix world;
    matrix bones[256];
};

cbuffer Material : register(b2)
{
    float4 diffuse;
    float4 specular;
    float3 ambient;
}

Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
Texture2D<float4> spa : register(t2);
Texture2D<float4> toon : register(t3);

SamplerState smp : register(s0);
SamplerState smpToon : register(s1);