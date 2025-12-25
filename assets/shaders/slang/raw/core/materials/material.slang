module material;

__include "standardPbr";

import "core/attributes";

/// Represents resources for standard pbr materials
/// It is assumed to be in a separate `ParameterBlock`
public struct StandardPbrMaterials {
    public StructuredBuffer<StandardPbrMaterial> materials;

    [Bindless]
    public Texture2D textures[];
}