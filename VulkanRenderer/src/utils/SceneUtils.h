#pragma once

#include <string_view>
#include <vector>

class DescriptorAllocator;
class DescriptorLayoutCache;
class Scene;
class ShaderPipelineTemplate;

namespace sceneUtils
{
    ShaderPipelineTemplate* loadShaderPipelineTemplate(const std::vector<std::string_view>& paths,
        std::string_view templateName,
        Scene& scene, DescriptorAllocator& allocator,
        DescriptorLayoutCache& layoutCache);
}
