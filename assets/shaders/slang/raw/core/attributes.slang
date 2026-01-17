module attributes;

public enum BindlessHandleKind
{
    Texture = 0,
}

[__AttributeUsage(_AttributeTargets.Struct)]
struct BindlessHandleAttribute {
    BindlessHandleKind kind;
}

[Flags]
public enum SamplerFlags
{
    None = 0,
    Linear = 4,
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

[__AttributeUsage(_AttributeTargets.Var)]
struct DefaultValueAttribute {
    string value;
}