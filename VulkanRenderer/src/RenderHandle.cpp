#include "rendererpch.h"

#include "RenderHandle.h"

#include "RenderObject.h"
#include "Rendering/Image/Image.h"

static_assert(RenderHandle<Texture>::NON_HANDLE == MaterialGPU::NO_TEXTURE,
              "Non handle of texture must be equal to the `no texture` value of material");

static_assert(std::is_same_v<RenderHandle<Texture>::UnderlyingType, u32>,
    "Underlying type of handle must be u32");

static_assert(sizeof(RenderHandle<Texture>) == sizeof(RenderHandle<Texture>::UnderlyingType),
    "Size of handle must be equal to the size of underlying type");