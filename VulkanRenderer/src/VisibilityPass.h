#pragma once

#include "Vulkan/Image.h"
#include "Vulkan/Shader.h"

#include <glm/glm.hpp>

#include "Vulkan/RenderingInfo.h"
#include "Vulkan/Synchronization.h"

class RenderPassGeometry;
class RenderPassGeometryCull;
class DepthPyramid;
struct FrameContext;

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
    RenderPassGeometry* RenderPassGeometry;
    RenderPassGeometryCull* RenderPassGeometryCull;
};

struct VisibilityRenderInfo
{
    FrameContext* FrameContext;
    const Image* DepthBuffer;
};

class VisibilityPass
{
public:
    bool Init(const VisibilityPassInitInfo& initInfo);
    void Shutdown();

    void RenderVisibility(const VisibilityRenderInfo& renderInfo);

    const Texture& GetVisibilityImage() const { return m_VisibilityBuffer->GetVisibilityImage(); }
    
private:
    RenderingInfo GetClearRenderingInfo(const Image& depthBuffer, const glm::uvec2& resolution) const;
    RenderingInfo GetLoadRenderingInfo(const Image& depthBuffer, const glm::uvec2& resolution) const;
private:
    ShaderPipelineTemplate* m_Template{};
    ShaderPipeline m_Pipeline{};
    ShaderDescriptorSet m_DescriptorSet{};

    std::unique_ptr<VisibilityBuffer> m_VisibilityBuffer{};

    RenderPassGeometry* m_RenderPassGeometry{nullptr};
    RenderPassGeometryCull* m_RenderPassGeometryCull{nullptr};
};