struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

float4 SimplePixelShader(PixelShaderInput IN): SV_TARGET
{
    return IN.color;
}