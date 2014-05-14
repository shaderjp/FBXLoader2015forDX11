cbuffer cbGlobal : register( b0 )
{
	matrix World;
    matrix View;
	matrix Projection;
	matrix WVP;
};

struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Nor : NORMAL;
	float2 Tex : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD;
};

VS_OUTPUT vs_main(VS_INPUT input, uint instanceID : SV_InstanceID)
{
	VS_OUTPUT output;

    output.Pos = mul( input.Pos, WVP );
	output.Tex = input.Tex;
	return output;
}