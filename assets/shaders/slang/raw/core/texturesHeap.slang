module textureHeap;

import "core/attributes";

[StandaloneType("textureHandle")]
[BindlessHandle(BindlessHandleKind.Texture)]
public struct TextureHandle <T> where T : IOpaqueDescriptor {
    private uint handle;
    public property DescriptorHandle<T> desc
    {
        [ForceInline]
        get {
            return (DescriptorHandle<T>)uint2(handle, 0);
        }
    }
}