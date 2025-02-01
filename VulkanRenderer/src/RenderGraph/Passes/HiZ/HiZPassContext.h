#pragma once

#include "Settings.h"
#include "RenderGraph/RGResource.h"

#include <array>

enum class HiZReductionMode { Min = 0, Max = 1, MaxVal };

class HiZPassContext
{
public:
    static constexpr i8 MAX_MIPMAP_COUNT = 16;
    HiZPassContext(const glm::uvec2& resolution, DeletionQueue& deletionQueue);

    void SetHiZResource(RG::Resource hiz, HiZReductionMode mode) { m_HiZResources[(u32)mode] = hiz; }
    RG::Resource GetHiZResource(HiZReductionMode mode) const { return m_HiZResources[(u32)mode]; }
    Texture GetHiZ(HiZReductionMode mode) const { return m_HiZs[(u32)mode]; }
    std::shared_ptr<Texture>* GetHiZPrevious(HiZReductionMode mode) { return &m_HiZsPrevious[(u32)mode]; }
    Texture GetHiZPrevious(HiZReductionMode mode) const { return *m_HiZsPrevious[(u32)mode]; }
    Sampler GetMinMaxSampler(HiZReductionMode mode) const { return m_MinMaxSamplers[(u32)mode]; }
    Span<const ImageSubresourceDescription> GetViewHandles() const { return m_MipmapViews; }

    /* NOTE: this is not hiz resolution (which is a power of 2), but the resolution that was passed into constructor */
    const glm::uvec2& GetDrawResolution() const { return m_DrawResolution; }
    const glm::uvec2& GetHiZResolution() const { return m_HiZResolution; }

    Buffer GetMinMaxDepthBuffer() const { return m_MinMaxDepth[m_FrameNumber]; }
    Buffer GetPreviousMinMaxDepthBuffer() const { return m_MinMaxDepth[PreviousFrame()]; }
    
    void NextFrame() { m_FrameNumber = (m_FrameNumber + 1) % BUFFERED_FRAMES; }
private:
    u32 PreviousFrame() const { return (m_FrameNumber + BUFFERED_FRAMES - 1) % BUFFERED_FRAMES; }
private:
    std::array<RG::Resource, (u32)HiZReductionMode::MaxVal> m_HiZResources;
    std::array<Texture, (u32)HiZReductionMode::MaxVal> m_HiZs;
    std::array<std::shared_ptr<Texture>, (u32)HiZReductionMode::MaxVal> m_HiZsPrevious;
    std::array<Sampler, (u32)HiZReductionMode::MaxVal> m_MinMaxSamplers;

    /* is detached from real frame number */
    u32 m_FrameNumber{0};
    std::array<Buffer, BUFFERED_FRAMES> m_MinMaxDepth;
    
    Span<const ImageSubresourceDescription> m_MipmapViews;
    
    glm::uvec2 m_DrawResolution{};
    glm::uvec2 m_HiZResolution{};
};
