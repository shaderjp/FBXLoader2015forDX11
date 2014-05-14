Texture2D txDiffuse : register( t0 );
SamplerState samLinear : register( s0 );

cbuffer cbMaterial : register( b0 )
{
	float4 ambient;
	float4 diffuse;
	float3 specular;
	float power;
	float4 emmisive;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD;
};

float4 PS( PS_INPUT input) : SV_Target
{
	return txDiffuse.Sample( samLinear, input.Tex );
//	return diffuse;
}