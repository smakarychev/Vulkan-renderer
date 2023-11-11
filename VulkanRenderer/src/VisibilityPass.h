#pragma once

#include "Vulkan/Image.h"
#include "Vulkan/Shader.h"

#include <glm/glm.hpp>

class SceneCull;
class SceneCullBuffers;
class Scene;

class VisibilityBuffer
{
public:
    VisibilityBuffer(const glm::uvec2& size, const CommandBuffer& cmd);
    ~VisibilityBuffer();

    const Image& GetVisibilityImage() const { return m_VisibilityImage; }
private:
    Image m_VisibilityImage;
};

struct VisibilityPassInitInfo
{
    glm::vec2 Size;
    const CommandBuffer* Cmd;
    Scene* Scene;
    DescriptorAllocator* DescriptorAllocator;
    DescriptorLayoutCache* LayoutCache;
    RenderingDetails RenderingDetails;
};

class VisibilityPass
{
public:
    void Init(const VisibilityPassInitInfo& initInfo);
    void ShutDown();
private:
    ShaderPipelineTemplate* m_Template{};
    ShaderPipeline m_Pipeline{};
    ShaderDescriptorSet m_DescriptorSet{};

    std::unique_ptr<VisibilityBuffer> m_VisibilityBuffer{};
};