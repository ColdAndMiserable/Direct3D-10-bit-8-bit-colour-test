

struct VertexShaderInput
{
    float3 pos : POSITION;
};

struct PixelShaderInput
{
    float4 pos : SV_POSITION;   //x,y,z,w
};

PixelShaderInput SimpleVertexShader(VertexShaderInput input)
{
    PixelShaderInput vertexShaderOutput;

    vertexShaderOutput.pos = float4(input.pos, 1.0f); //x,y,depth,scaling
    return vertexShaderOutput;
}