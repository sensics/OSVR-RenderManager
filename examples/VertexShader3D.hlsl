cbuffer cbPerObject
{
    matrix projectionMatrix;
    matrix viewMatrix;
    matrix worldMatrix;
}

struct VertexShaderInput
{
    float4 position : POSITION;
    float4 color: COLOR0;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

VertexShaderOutput SimpleVertexShader(VertexShaderInput IN)
{
    VertexShaderOutput output;
    matrix wvp = mul(projectionMatrix, mul(viewMatrix, worldMatrix));
    output.position = mul(wvp, IN.position);
    output.color = IN.color;
    return output;
}