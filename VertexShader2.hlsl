

struct VertexShaderInput
{
    float3 pos : POSITION;
    float2 tex : TEXCOORD0;
};

struct PixelShaderInput
{
    float2 tex : TEXCOORD0;
    float4 pos : SV_POSITION;   //x,y,z,w

};

PixelShaderInput SimpleVertexShader(VertexShaderInput input)
{
    PixelShaderInput vertexShaderOutput;

    vertexShaderOutput.pos = float4(input.pos,1.0f); //x,y,depth,scaling
    vertexShaderOutput.tex = float2(input.tex);
    return vertexShaderOutput;
}