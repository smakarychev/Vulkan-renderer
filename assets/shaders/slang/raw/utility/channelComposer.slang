module channelComposer;

namespace channelComposer {
    public enum Channel {R, G, B, A, One, Zero};

    float project<let PickChannel : Channel>(float4 color) {
        switch (PickChannel) {
        case Channel.R: 
            return color.r;
        case Channel.G:
            return color.g;
        case Channel.B:
            return color.b;
        case Channel.A:
            return color.a;
        case Channel.One:
            return 1.0f;
        case Channel.Zero:
            return 0.0f;
        }
        return 1.0f;
    }

    public float4 compose<
        let RChannel : Channel,
        let GChannel : Channel,
        let BChannel : Channel,
        let AChannel : Channel>(float4 color) {

        float4 outColor;
        outColor.r = project<RChannel>(color);
        outColor.g = project<GChannel>(color);
        outColor.b = project<BChannel>(color);
        outColor.a = project<AChannel>(color);

        return outColor;
    } 

    public float4 composeRGBA(float4 color) {
        return compose<Channel.R, Channel.G, Channel.B, Channel.A>(color);
    }

    public float4 composeRGB(float4 color) {
        return compose<Channel.R, Channel.G, Channel.B, Channel.One>(color);
    }

    public float4 composeR(float4 color) {
        return compose<Channel.R, Channel.R, Channel.R, Channel.One>(color);
    }

    public float4 composeG(float4 color) {
        return compose<Channel.G, Channel.G, Channel.G, Channel.One>(color);
    }

    public float4 composeB(float4 color) {
        return compose<Channel.B, Channel.B, Channel.B, Channel.One>(color);
    }

    public float4 composeRG(float4 color) {
        return compose<Channel.R, Channel.G, Channel.Zero, Channel.One>(color);
    }

    public float4 composeRB(float4 color) {
        return compose<Channel.R, Channel.Zero, Channel.B, Channel.One>(color);
    }

    public float4 composeGB(float4 color) {
        return compose<Channel.Zero, Channel.G, Channel.B, Channel.One>(color);
    }
}