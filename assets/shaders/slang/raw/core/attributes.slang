module attributes;

[__AttributeUsage(_AttributeTargets.Var)]
struct BindlessAttribute {}


[Flags]
public enum SamplerFlags
{
    None = 0,
    Linear = 2,
    Nearest,
    ClampEdge,
    ClampBlack,
    ClampWhite,
    Shadow,
    ReductionMin,
    ReductionMax
}

[__AttributeUsage(_AttributeTargets.Var)]
struct ImmutableSamplerAttribute {
    SamplerFlags flags = SamplerFlags.None;
}

[__AttributeUsage(_AttributeTargets.Struct)]
struct StandaloneTypeAttribute {
    string name;
}