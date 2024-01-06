#pragma once

#include "Vulkan/Image.h"
#include "Vulkan/Shader.h"

#include <glm/glm.hpp>

#include "Vulkan/RenderingInfo.h"
#include "Vulkan/Syncronization.h"

class DepthPyramid;
struct FrameContext;
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
    DescriptorAllocator* DescriptorAllocator;
    DescriptorLayoutCache* LayoutCache;
    RenderingDetails RenderingDetails;
    const Buffer* CameraBuffer;
    const Buffer* CommandsBuffer;
    const Buffer* ObjectsBuffer;
    const Buffer* TrianglesBuffer;
    const Buffer* MaterialsBuffer;
    const Scene* Scene;
};

struct VisibilityRenderInfo
{
    const Scene* Scene;
    SceneCull* SceneCull;
    FrameContext* FrameContext;
    DepthPyramid* DepthPyramid;
    const Image* DepthBuffer;
};

class VisibilityPass
{
public:
    bool Init(const VisibilityPassInitInfo& initInfo);
    void ShutDown();

    void RenderVisibility(const VisibilityRenderInfo& renderInfo);

    const Texture& GetVisibilityImage() const { return m_VisibilityBuffer->GetVisibilityImage(); }
    
private:
    RenderingInfo GetClearRenderingInfo(const Image& depthBuffer, const glm::uvec2& resolution) const;
    RenderingInfo GetLoadRenderingInfo(const Image& depthBuffer, const glm::uvec2& resolution) const;
    void ComputeDepthPyramid(const CommandBuffer& cmd, DepthPyramid& depthPyramid, const Image& depthBuffer);
    void RenderScene(const CommandBuffer& cmd, const Scene& scene, const SceneCull& sceneCull, u32 frameNumber);
private:
    ShaderPipelineTemplate* m_Template{};
    ShaderPipeline m_Pipeline{};
    ShaderDescriptorSet m_DescriptorSet{};

    TimelineSemaphore m_CullSemaphore;
    TimelineSemaphore m_RenderSemaphore;

    std::unique_ptr<VisibilityBuffer> m_VisibilityBuffer{};
};