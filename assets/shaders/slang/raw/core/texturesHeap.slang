module textureHeap;

import "core/attributes";

public struct TextureHeap {
    [Bindless]
    public Texture2D textures[];
}