

cbuffer VS_CONSTANT_BUFFER : register(b0)
{
	uint offset;
	float shade;
	float a;
	float b;
	float R, G, B;
	unsigned int padding3;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;	//input SV_POSITION is the x,y pixel position starting at (0,0)
	uint pid : SV_PrimitiveID;	//primitive identifier
};



float4 SimplePixelShader(PixelShaderInput input) : SV_TARGET
{
float square = (float)input.pid - (float)offset - (float)((input.pid - offset) % 2);
precise float s = square * shade;
precise float r = s * R;
precise float g = s * G;
precise float b = s * B;
return float4(r,g,b,1.0f);
}

