

//Globals
Texture2D shaderTexture;
SamplerState SampleType;

//Sampler State confirmed working. Texture input coordinates wrong.
struct PixelShaderInput
{
	float2 tex : TEXCOORD0;
	float4 pos : SV_POSITION;
};


float4 SimplePixelShader(PixelShaderInput input) : SV_TARGET
{
float4 textureColor;
textureColor = shaderTexture.Sample(SampleType, input.tex);
return(textureColor);
}