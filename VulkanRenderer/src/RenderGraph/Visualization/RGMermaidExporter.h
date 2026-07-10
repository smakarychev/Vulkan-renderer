#pragma once

#include "RenderGraph/RGGraphWatcher.h"

#include <filesystem>
#include <sstream>
#include <unordered_set>

namespace RG
{
class RGMermaidExporter final : public GraphWatcher
{
public:
    void OnPassOrderFinalized(const std::vector<std::unique_ptr<Pass>>& passes) override;
    void OnBufferResourcesFinalized(const std::vector<RGBuffer>& buffers) override;
    void OnImageResourcesFinalized(const std::vector<RGImage>& images) override;
    void OnBarrierAdded(const BufferBarrier& barrierInfo, const Pass& firstPass, const Pass& secondPass) override;
    void OnBarrierAdded(const ImageBarrier& barrierInfo, const Pass& firstPass, const Pass& secondPass) override;
    void OnReset() override;

    void ExportToHtml(const std::filesystem::path& outputPath) const;
private:
    void AddResourceToStream(const Pass& firstPass, const Pass& secondPass, u32 resourceIndex, std::string_view tag);
private:
    std::stringstream m_Stream;
    std::unordered_map<u32, std::string> m_BufferIndexToDescription;
    std::unordered_map<u32, std::string> m_ImageIndexToDescription;
    std::unordered_set<u32> m_DumpedBufferDescriptions;
    std::unordered_set<u32> m_DumpedImageDescriptions;
};
}
