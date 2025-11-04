module fullscreen;

public struct FullScreenVSOutput
{
    public float2 UV;
    public float4 Position : SV_POSITION;
};

[shader("vertex")]
FullScreenVSOutput fullscreenMain(uint VertexIndex: SV_VertexID)
{
    FullScreenVSOutput output;
    output.UV = float2(VertexIndex & 2, (VertexIndex << 1) & 2);
    output.Position = float4(output.UV * 2.0f - 1.0f, 0.0f, 1.0f);
 
    return output;
}