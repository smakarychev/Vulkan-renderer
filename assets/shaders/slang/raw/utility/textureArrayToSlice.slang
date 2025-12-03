module textureArrayToSlice;

import "utility/fullscreen";
import "utility/channelComposer";
import "core/attributes";

struct Samplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct Resources {
    Texture2DArray textureArray;
}

ParameterBlock<Samplers> samplers;
ParameterBlock<Resources> resources;

#ifndef R_CHANNEL
#define R_CHANNEL uint(channelComposer.Channel.R)
#endif // R_CHANNEL 
#ifndef G_CHANNEL
#define G_CHANNEL uint(channelComposer.Channel.G)
#endif // G_CHANNEL 
#ifndef B_CHANNEL
#define B_CHANNEL uint(channelComposer.Channel.B)
#endif // B_CHANNEL
#ifndef A_CHANNEL
#define A_CHANNEL uint(channelComposer.Channel.A)
#endif // A_CHANNEL

[shader("pixel")]
float4 main(FullScreenVSOutput input, uniform uint layer) {
    using channelComposer;

    const float4 color = resources.textureArray.SampleLevel(samplers.sampler, float3(input.uv, layer), 0).rgba;
    return channelComposer.compose<
        (Channel)R_CHANNEL,
        (Channel)G_CHANNEL,
        (Channel)B_CHANNEL,
        (Channel)A_CHANNEL>(color); 
}