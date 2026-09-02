#include "BasicShaderHeader.hlsli"

float4 BasicPS(Output input) : SV_TARGET
{
    float3 light = normalize(float3(1, -1, 1));
    float3 lightColor = float3(1, 1, 1);
    
    float diffuseB = saturate(dot(-light, input.normal));
    float4 toonDiff = toon.Sample(smpToon, float2(0, 1.0 - diffuseB));
    float3 refLight = normalize(reflect(light, input.normal.xyz));
    float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);
    
    float2 sphereMapUV = input.vnormal.xy;
    sphereMapUV = (sphereMapUV + float2(1, -1)) * float2(0.5, -0.5);
    
    float4 texColor = tex.Sample(smp, input.uv);
    
    return max(saturate(
        toonDiff
        * diffuse
        * texColor
        * sph.Sample(smp, sphereMapUV))
        + saturate(spa.Sample(smp, sphereMapUV) * texColor
        + float4(specularB * specular.rgb, 1))
        , float4(texColor * ambient, 1));
}