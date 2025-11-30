module fullscreen;

public struct FullScreenVSOutput
{
    public float2 uv;
    public float4 position : SV_POSITION;
};

[shader("vertex")]
FullScreenVSOutput fullscreenMain(uint VertexIndex: SV_VertexID)
{
    FullScreenVSOutput output;
    output.uv = float2(VertexIndex & 2, (VertexIndex << 1) & 2);
    output.position = float4(output.uv * 2.0f - 1.0f, 0.0f, 1.0f);
 
    return output;
}