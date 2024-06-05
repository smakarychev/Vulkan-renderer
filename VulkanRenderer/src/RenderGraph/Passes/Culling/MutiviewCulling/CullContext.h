#pragma once

#include "RenderGraph/RGDrawResources.h"
#include "Rendering/Buffer.h"

class SceneGeometry;

class CullViewVisibility
{
public:
    CullViewVisibility(const SceneGeometry& geometry);

    const Buffer& Mesh() const { return m_Mesh; }
    const Buffer& Meshlet() const { return m_Meshlet; }

    void NextFrame() { m_FrameNumber = (m_FrameNumber + 1) % BUFFERED_FRAMES; }
    const Buffer& CompactCount() const { return m_CompactCount[m_FrameNumber]; }
    u32 ReadbackCompactCountValue();
    u32 CompactCountValue() const { return m_CompactCountValue; }

    const SceneGeometry* Geometry() const { return m_Geometry; }
private:
    u32 ReadbackCount(const Buffer& buffer) const;
    u32 PreviousFrame() const { return (m_FrameNumber + BUFFERED_FRAMES - 1) % BUFFERED_FRAMES; }
private:
    Buffer m_Mesh;
    Buffer m_Meshlet;

    /* is detached from real frame number */
    u32 m_FrameNumber{0};
    std::array<Buffer, BUFFERED_FRAMES> m_CompactCount;
    u32 m_CompactCountValue{0};

    const SceneGeometry* m_Geometry{nullptr};
};

class CullViewTriangleVisibility
{
public:
    CullViewTriangleVisibility(CullViewVisibility* cullViewVisibility);

    const Buffer& Mesh() const { return m_CullViewVisibility->Mesh(); }
    const Buffer& Meshlet() const { return m_CullViewVisibility->Meshlet(); }
    const Buffer& Triangle() const { return m_Triangle; }

    void NextFrame() { m_CullViewVisibility->NextFrame(); }
    const Buffer& CompactCount() const { return m_CullViewVisibility->CompactCount(); }
    u32 ReadbackCompactCountValue() { return m_CullViewVisibility->ReadbackCompactCountValue(); }
    u32 CompactCountValue() const { return m_CullViewVisibility->CompactCountValue(); }
private:
    CullViewVisibility* m_CullViewVisibility{nullptr};

    Buffer m_Triangle;

    const SceneGeometry* m_Geometry{nullptr};
};

class TriangleCullMultiviewTraits
{
public:
    static constexpr u32 MAX_BATCHES = 2;
    static constexpr u32 MAX_TRIANGLES = 128'000;
    static constexpr u32 MAX_INDICES = MAX_TRIANGLES * 3;
    static constexpr u32 MAX_COMMANDS = MAX_TRIANGLES / assetLib::ModelInfo::TRIANGLES_PER_MESHLET;
    using TriangleType = u8;
    using IndexType = u32;
public:
    static u32 TriangleCount() { return MAX_TRIANGLES * SUB_BATCH_COUNT; }
    static u32 IndexCount() { return MAX_INDICES * SUB_BATCH_COUNT; }
    static u32 CommandCount() { return MAX_COMMANDS * SUB_BATCH_COUNT; }
    static u32 MaxDispatches(u32 commandCount) { return commandCount / CommandCount() + 1; }
};

