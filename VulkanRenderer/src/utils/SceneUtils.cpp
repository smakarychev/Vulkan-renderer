#include "SceneUtils.h"

#include "Scene.h"
#include "Vulkan/Shader.h"

namespace sceneUtils
{
    ShaderPipelineTemplate* loadShaderPipelineTemplate(const std::vector<std::string_view>& paths,
    std::string_view templateName,
    Scene& scene, DescriptorAllocator& allocator,
    DescriptorLayoutCache& layoutCache)
    {
        if (!scene.GetShaderTemplate(std::string{templateName}))
        {
            Shader* shaderReflection = Shader::ReflectFrom(paths);

            ShaderPipelineTemplate shaderTemplate = ShaderPipelineTemplate::Builder()
                .SetDescriptorAllocator(&allocator)
                .SetDescriptorLayoutCache(&layoutCache)
                .SetShaderReflection(shaderReflection)
                .Build();
        
            scene.AddShaderTemplate(shaderTemplate, std::string{templateName});
        }
    
        return scene.GetShaderTemplate(std::string{templateName});
    }
}


