module fullscreen;

public struct FullScreenVSOutput
{
    public float2 uv;
    public float2 position;
    public float4 positionSV : SV_POSITION;
};

[shader("vertex")]
FullScreenVSOutput fullscreenMain(uint VertexIndex: SV_VertexID)
{
    FullScreenVSOutput output;
    output.uv = float2(VertexIndex & 2, (VertexIndex << 1) & 2);
    output.position = output.uv * 2.0f - 1.0f;
    output.positionSV = float4(output.position, 0.0f, 1.0f);

    return output;
}