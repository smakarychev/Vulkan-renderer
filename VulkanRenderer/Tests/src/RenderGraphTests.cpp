#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGGraphWatcher.h"


// NOLINTBEGIN

using enum RG::ResourceAccessFlags;

class TestGraphWatcher final : public RG::GraphWatcher
{
public:
    void OnPassOrderFinalized(const std::vector<std::unique_ptr<RG::Pass>>& passes) override
    {
        Passes = &passes;
    }
    void OnBufferResourcesFinalized(const std::vector<RG::BufferResource>& buffers) override
    {
        Buffers = &buffers;
    }
    void OnImageResourcesFinalized(const std::vector<RG::ImageResource>& images) override
    {
        Images = &images;
    }
    void OnBufferAccessesFinalized(const std::vector<RG::ResourceAccess>& accesses) override
    {
        BufferAccesses = &accesses;
    }
    void OnImagesAccessesFinalized(const std::vector<RG::ResourceAccess>& accesses) override
    {
        ImageAccesses = &accesses;
    }
    void OnBarrierAdded(const BarrierInfo& barrierInfo, const RG::Pass& firstPass, const RG::Pass& secondPass) override
    {
        Barriers.push_back({
            .BarrierType = barrierInfo.BarrierType,
            .Resource = barrierInfo.Resource,
            .DependencyInfo = *barrierInfo.DependencyInfo,
            .FirstPass = &firstPass,
            .SecondPass = &secondPass,
        });
    }
    
    const std::vector<std::unique_ptr<RG::Pass>>* Passes;
    const std::vector<RG::BufferResource>* Buffers;
    const std::vector<RG::ImageResource>* Images;
    const std::vector<RG::ResourceAccess>* BufferAccesses;
    const std::vector<RG::ResourceAccess>* ImageAccesses;
    struct BarrierPass
    {
        BarrierInfo::Type BarrierType{BarrierInfo::Type::Barrier};
        RG::Resource Resource{};
        DependencyInfoCreateInfo DependencyInfo{};
        const RG::Pass* FirstPass{};
        const RG::Pass* SecondPass{};
    };
    std::vector<BarrierPass> Barriers;
};

TEST_CASE("RenderGraphResource Creation", "[RenderGraph][Resource]")
{
    SECTION("Is invalid by default")
    {
        RG::Resource resource = {};
        REQUIRE_FALSE(resource.IsValid());
    }
    SECTION("Invalid resource returns `Invalid` as string")
    {
        RG::Resource resource = {};
        std::string resourceString = resource.AsString();
        REQUIRE(resourceString == "Invalid");
        resourceString = std::format("{}", resource);
        REQUIRE(resourceString == "Invalid");
    }
    SECTION("Can create buffer resource")
    {
        RG::Graph graph;
        RG::Resource resource = graph.Create("MyBuffer"_hsv, RG::RGBufferDescription{.SizeBytes = 2});
        REQUIRE(resource.IsValid());
        REQUIRE(resource.IsBuffer());
        REQUIRE_THAT(resource.AsString(), Catch::Matchers::StartsWith("Buffer"));
        const BufferDescription& description = graph.GetBufferDescription(resource);
        REQUIRE(description.SizeBytes == 2);
    }
    SECTION("Can create image resource")
    {
        RG::Graph graph;
        RG::Resource resource = graph.Create("MyImage"_hsv, RG::RGImageDescription{
            .Width = 640,
            .Height = 480,
            .Format = Format::RGBA16_FLOAT
        });
        REQUIRE(resource.IsValid());
        REQUIRE(resource.IsImage());
        REQUIRE_THAT(resource.AsString(), Catch::Matchers::StartsWith("Image"));
        const ImageDescription& description = graph.GetImageDescription(resource);
        REQUIRE(description.Width == 640);
        REQUIRE(description.Height == 480);
        REQUIRE(description.Format == Format::RGBA16_FLOAT);
    }
    SECTION("Can create image resource from reference")
    {
        RG::Graph graph;
        RG::Resource reference = graph.Create("Reference"_hsv, RG::RGImageDescription{
            .Width = 640,
            .Height = 480,
            .Format = Format::RGBA16_FLOAT
        });
        RG::RGImageDescription defaultDescription = {
            .Width = 0.5f,
            .Height = 1.0f,
            .LayersDepth = 2.0f,
            .Reference = reference,
            .Format = Format::RGBA8_SINT,
            .Kind = ImageKind::Image3d,
            .MipmapFilter = ImageFilter::Nearest};
        {
            defaultDescription.Inference = RG::RGImageInference::Size;
            RG::Resource resource = graph.Create("MyImage"_hsv, defaultDescription);
            REQUIRE(resource.IsValid());
            REQUIRE(resource.IsImage());
            const ImageDescription& description = graph.GetImageDescription(resource);
            REQUIRE(description.Width == 320);
            REQUIRE(description.Height == 480);
            REQUIRE(description.LayersDepth == 2);
            REQUIRE(description.Format == Format::RGBA8_SINT);
            REQUIRE(description.Kind == ImageKind::Image3d);
            REQUIRE(description.MipmapFilter == ImageFilter::Nearest);
        }
        {
            defaultDescription.Inference = RG::RGImageInference::Size2d;
            RG::Resource resource = graph.Create("MyImage"_hsv, defaultDescription);
            REQUIRE(resource.IsValid());
            REQUIRE(resource.IsImage());
            const ImageDescription& description = graph.GetImageDescription(resource);
            REQUIRE(description.Width == 320);
            REQUIRE(description.Height == 480);
            REQUIRE(description.LayersDepth == 2.0f);
            REQUIRE(description.Format == Format::RGBA8_SINT);
            REQUIRE(description.Kind == ImageKind::Image3d);
            REQUIRE(description.MipmapFilter == ImageFilter::Nearest);
        }
        {
            defaultDescription.Inference = RG::RGImageInference::Depth;
            RG::Resource resource = graph.Create("MyImage"_hsv, defaultDescription);
            REQUIRE(resource.IsValid());
            REQUIRE(resource.IsImage());
            const ImageDescription& description = graph.GetImageDescription(resource);
            REQUIRE(description.Width == 0);
            REQUIRE(description.Height == 1);
            REQUIRE(description.LayersDepth == 2);
            REQUIRE(description.Format == Format::RGBA8_SINT);
            REQUIRE(description.Kind == ImageKind::Image3d);
            REQUIRE(description.MipmapFilter == ImageFilter::Nearest);
        }
        {
            defaultDescription.Inference = RG::RGImageInference::Format;
            RG::Resource resource = graph.Create("MyImage"_hsv, defaultDescription);
            REQUIRE(resource.IsValid());
            REQUIRE(resource.IsImage());
            const ImageDescription& description = graph.GetImageDescription(resource);
            REQUIRE(description.Width == 0);
            REQUIRE(description.Height == 1);
            REQUIRE(description.LayersDepth == 2);
            REQUIRE(description.Format == Format::RGBA16_FLOAT);
            REQUIRE(description.Kind == ImageKind::Image3d);
            REQUIRE(description.MipmapFilter == ImageFilter::Nearest);
        }
        {
            defaultDescription.Inference = RG::RGImageInference::Kind;
            RG::Resource resource = graph.Create("MyImage"_hsv, defaultDescription);
            REQUIRE(resource.IsValid());
            REQUIRE(resource.IsImage());
            const ImageDescription& description = graph.GetImageDescription(resource);
            REQUIRE(description.Width == 0);
            REQUIRE(description.Height == 1);
            REQUIRE(description.LayersDepth == 2);
            REQUIRE(description.Format == Format::RGBA8_SINT);
            REQUIRE(description.Kind == ImageKind::Image2d);
            REQUIRE(description.MipmapFilter == ImageFilter::Nearest);
        }
        {
            defaultDescription.Inference = RG::RGImageInference::Filter;
            RG::Resource resource = graph.Create("MyImage"_hsv, defaultDescription);
            REQUIRE(resource.IsValid());
            REQUIRE(resource.IsImage());
            const ImageDescription& description = graph.GetImageDescription(resource);
            REQUIRE(description.Width == 0);
            REQUIRE(description.Height == 1);
            REQUIRE(description.LayersDepth == 2);
            REQUIRE(description.Format == Format::RGBA8_SINT);
            REQUIRE(description.Kind == ImageKind::Image3d);
            REQUIRE(description.MipmapFilter == ImageFilter::Linear);
        }
        {
            defaultDescription.Inference = RG::RGImageInference::Full;
            RG::Resource resource = graph.Create("MyImage"_hsv, defaultDescription);
            REQUIRE(resource.IsValid());
            REQUIRE(resource.IsImage());
            const ImageDescription& description = graph.GetImageDescription(resource);
            REQUIRE(description.Width == 320);
            REQUIRE(description.Height == 480);
            REQUIRE(description.LayersDepth == 2);
            REQUIRE(description.Format == Format::RGBA16_FLOAT);
            REQUIRE(description.Kind == ImageKind::Image2d);
            REQUIRE(description.MipmapFilter == ImageFilter::Linear);
        }
    }
    SECTION("Resources are virtual and not created immediately")
    {
        RG::Graph graph;
        RG::Resource buffer = graph.Create("MyBuffer"_hsv, RG::RGBufferDescription{});
        REQUIRE(buffer.IsValid());
        REQUIRE(!graph.GetBuffer(buffer).HasValue());
        RG::Resource image = graph.Create("MyImage"_hsv, RG::RGImageDescription{});
        REQUIRE(image.IsValid());
        REQUIRE(!graph.GetImage(image).HasValue());
    }
    SECTION("Imported resources have physical value")
    {
        Device::Init(DeviceCreateInfo::Default(nullptr, true));
        RG::Graph graph;
        Buffer buffer = Device::CreateBuffer({.SizeBytes = 4, .Usage = BufferUsage::Ordinary | BufferUsage::Uniform});
        RG::Resource bufferResource = graph.Import("MyBuffer"_hsv, buffer);
        REQUIRE(bufferResource.IsValid());
        REQUIRE(graph.GetBuffer(bufferResource).HasValue());
        REQUIRE(graph.GetBuffer(bufferResource) == buffer);

        Image image = Device::CreateImage({
            .Description = {
                .Width = 100,
                .Height = 200,
                .Format = Format::RGBA16_FLOAT,
                .Usage = ImageUsage::Color
            },
            .CalculateMipmaps = false});
        RG::Resource imageResource = graph.Import("MyImage"_hsv, image);
        REQUIRE(imageResource.IsValid());
        REQUIRE(graph.GetImage(imageResource).HasValue());
        REQUIRE(graph.GetImage(imageResource) == image);
    }
    SECTION("Can not split external image into views")
    {
        Device::Init(DeviceCreateInfo::Default(nullptr, true));
        RG::Graph graph;
        Image image = Device::CreateImage({
            .Description = {
                .Width = 100,
                .Height = 200,
                .Format = Format::RGBA16_FLOAT,
                .Usage = ImageUsage::Color
            },
            .CalculateMipmaps = false});
        RG::Resource imageResource = graph.Import("MyImage"_hsv, image);
        RG::Resource failedSplit = graph.SplitImage(imageResource, {});
        REQUIRE(!failedSplit.IsValid());
    }
    SECTION("Can split image resource into views")
    {
        RG::Graph graph;
        RG::Resource image = graph.Create("MyImage"_hsv, RG::RGImageDescription{});
        REQUIRE(graph.GetImageDescription(image).AdditionalViews.empty());
        RG::Resource split0 = graph.SplitImage(image, {.LayerBase = 1});
        REQUIRE(graph.GetImageDescription(image).AdditionalViews.size() == 1);
        REQUIRE(&graph.GetImageDescription(image) == &graph.GetImageDescription(split0));
        RG::Resource split1 = graph.SplitImage(image, {.LayerBase = 2});
        REQUIRE(graph.GetImageDescription(image).AdditionalViews.size() == 2);
        REQUIRE(&graph.GetImageDescription(image) == &graph.GetImageDescription(split1));
    }
    SECTION("Identicals splits do result in single resource")
    {
        RG::Graph graph;
        RG::Resource image = graph.Create("MyImage"_hsv, RG::RGImageDescription{});
        REQUIRE(graph.GetImageDescription(image).AdditionalViews.empty());
        RG::Resource split0 = graph.SplitImage(image, {});
        REQUIRE(graph.GetImageDescription(image).AdditionalViews.size() == 1);
        REQUIRE(&graph.GetImageDescription(image) == &graph.GetImageDescription(split0));
        RG::Resource split1 = graph.SplitImage(image, {});
        REQUIRE(graph.GetImageDescription(image).AdditionalViews.size() == 1);
        REQUIRE(&graph.GetImageDescription(image) == &graph.GetImageDescription(split1));
    }
    SECTION("Can merge split image")
    {
        RG::Graph graph;
        RG::Resource image = graph.Create("MyImage"_hsv, RG::RGImageDescription{});
        RG::Resource image2 = graph.Create("MyImage2"_hsv, RG::RGImageDescription{});
        RG::Resource split0 = graph.SplitImage(image, {.LayerBase = 1});
        RG::Resource split1 = graph.SplitImage(image, {.LayerBase = 2});
        RG::Resource split2 = graph.SplitImage(image2, {.LayerBase = 3});

        RG::Resource failedDifferentResource = graph.MergeImage({split0, split2});
        REQUIRE(!failedDifferentResource.IsValid());
        RG::Resource failedExtraResources = graph.MergeImage({split0, split1, split2});
        REQUIRE(!failedExtraResources.IsValid());
        RG::Resource failedNotASplit = graph.MergeImage({image});
        REQUIRE(!failedNotASplit.IsValid());
        
        RG::Resource merged0 = graph.MergeImage({split0, split1});
        REQUIRE(merged0.IsValid());
        RG::Resource merged1 = graph.MergeImage({split2});
        REQUIRE(merged1.IsValid());
    }
}
TEST_CASE("RenderGraphResource Access", "[RenderGraph][Resource]")
{
    SECTION("Read of resource does not increment version")
    {
        RG::Graph renderGraph;
        renderGraph.AddRenderPass<u32>("MyPass"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                {
                    RG::Resource resource = graph.Create("MyBuffer"_hsv, RG::RGBufferDescription{.SizeBytes = 2});
                    REQUIRE_THAT(resource.AsString(), Catch::Matchers::EndsWith(".0)"));
                    resource = graph.ReadBuffer(resource,
                        Compute | Uniform);
                    REQUIRE_THAT(resource.AsString(), Catch::Matchers::EndsWith(".0)"));
                }
                {
                    RG::Resource resource = graph.Create("MyImage"_hsv, RG::RGImageDescription{
                        .Width = 640,
                        .Height = 480,
                        .Format = Format::RGBA16_FLOAT
                    });
                    REQUIRE_THAT(resource.AsString(), Catch::Matchers::EndsWith(".0)"));
                    resource = graph.ReadImage(resource, Compute | Sampled);
                    REQUIRE_THAT(resource.AsString(), Catch::Matchers::EndsWith(".0)"));
                }
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
    }
    SECTION("Write of resource does increment version")
    {
        RG::Graph renderGraph;
        renderGraph.AddRenderPass<u32>("MyPass"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                {
                    RG::Resource resource = graph.Create("MyBuffer"_hsv, RG::RGBufferDescription{.SizeBytes = 2});
                    REQUIRE_THAT(resource.AsString(), Catch::Matchers::EndsWith(".0)"));
                    resource = graph.WriteBuffer(resource, Compute | Storage);
                    REQUIRE_THAT(resource.AsString(), Catch::Matchers::EndsWith(".1)"));
                }
                {
                    RG::Resource resource = graph.Create("MyImage"_hsv, RG::RGImageDescription{
                        .Width = 640,
                        .Height = 480,
                        .Format = Format::RGBA16_FLOAT
                    });
                    REQUIRE_THAT(resource.AsString(), Catch::Matchers::EndsWith(".0)"));
                    resource = graph.WriteImage(resource, Compute | Storage);
                    REQUIRE_THAT(resource.AsString(), Catch::Matchers::EndsWith(".1)"));
                }
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
    }
    SECTION("ReadWrite of resource does increment version")
    {
        RG::Graph renderGraph;
        renderGraph.AddRenderPass<u32>("MyPass"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                {
                    RG::Resource resource = graph.Create("MyBuffer"_hsv, RG::RGBufferDescription{.SizeBytes = 2});
                    REQUIRE_THAT(resource.AsString(), Catch::Matchers::EndsWith(".0)"));
                    resource = graph.ReadWriteBuffer(resource, Compute | Storage);
                    REQUIRE_THAT(resource.AsString(), Catch::Matchers::EndsWith(".1)"));
                }
                {
                    RG::Resource resource = graph.Create("MyImage"_hsv, RG::RGImageDescription{
                        .Width = 640,
                        .Height = 480,
                        .Format = Format::RGBA16_FLOAT
                    });
                    REQUIRE_THAT(resource.AsString(), Catch::Matchers::EndsWith(".0)"));
                    resource = graph.ReadWriteImage(resource, Compute | Storage);
                    REQUIRE_THAT(resource.AsString(), Catch::Matchers::EndsWith(".1)"));
                }
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
    }
}
TEST_CASE("RenderGraph Passes", "[RenderGraph][Pass]")
{
    RG::Graph renderGraph;
    Device::Init(DeviceCreateInfo::Default(nullptr, true));
    FrameContext ctx = {};

    SECTION("Can create pass")
    {
        renderGraph.AddRenderPass<u32>("MyPass"_hsv,
            [&](RG::Graph& graph, u32& passData){},
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        SUCCEED();
    }
    SECTION("Can get pass output")
    {
        u32 data = renderGraph.AddRenderPass<u32>("MyPass"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                passData = 1;
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        REQUIRE(data == 1);
    }
    SECTION("Can write to splitted resource")
    {
        struct PassData
        {
            RG::Resource Resource{};
        };
        renderGraph.AddRenderPass<PassData>("Level0"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                RG::Resource image = graph.Create("MyImage"_hsv, RG::RGImageDescription{
                    .Width = 640,
                    .Height = 480,
                    .Mipmaps = 3,
                    .Format = Format::RGBA16_FLOAT});
                RG::Resource split0 = graph.SplitImage(image, {.MipmapBase = 1});
                RG::Resource split1 = graph.SplitImage(image, {.MipmapBase = 2});

                split0 = graph.WriteImage(split0, Compute | Storage);
                split1 = graph.WriteImage(split1, Compute | Storage);
                graph.WriteImage(split1, Compute | Storage);

                passData.Resource = image;

                renderGraph.Compile(ctx);
                SUCCEED();
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){});
    }
    SECTION("Can write to merged resource")
    {
        struct PassData
        {
            RG::Resource Resource{};
        };
        auto merged = renderGraph.AddRenderPass<PassData>("Level0"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                RG::Resource image = graph.Create("MyImage"_hsv, RG::RGImageDescription{
                    .Width = 640,
                    .Height = 480,
                    .Mipmaps = 3,
                    .Format = Format::RGBA16_FLOAT});
                RG::Resource split0 = graph.SplitImage(image, {.MipmapBase = 1});
                RG::Resource split1 = graph.SplitImage(image, {.MipmapBase = 2});

                split0 = graph.WriteImage(split0, Compute | Storage);
                split1 = graph.WriteImage(split1, Compute | Storage);
                split1 = graph.WriteImage(split1, Compute | Storage);

                passData.Resource =  graph.MergeImage({split0, split1});;
                REQUIRE(passData.Resource.IsValid());
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){}).Resource;
        
        renderGraph.AddRenderPass<PassData>("Level1"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Resource = graph.WriteImage(merged, Compute | Storage);
                REQUIRE(passData.Resource.IsValid());
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.Compile(ctx);
    }
    SECTION("Cannot access main splitted resource, before it was merged")
    {
        RG::Resource image = renderGraph.Create("MyImage"_hsv, RG::RGImageDescription{
            .Width = 640,
            .Height = 480,
            .Mipmaps = 3,
            .Format = Format::RGBA16_FLOAT});
        RG::Resource split0 = renderGraph.SplitImage(image, {.MipmapBase = 1});
        RG::Resource split1 = renderGraph.SplitImage(image, {.MipmapBase = 2});
        struct PassData
        {
            RG::Resource Resource{};
        };
        renderGraph.AddRenderPass<PassData>("Level0"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                RG::Resource failedRead = graph.ReadImage(image, Compute | Storage);
                REQUIRE(!failedRead.IsValid());

                RG::Resource failedWrite = graph.WriteImage(image, Compute | Storage);
                REQUIRE(!failedWrite.IsValid());

                RG::Resource merge = graph.MergeImage({split0, split1});
                RG::Resource read = graph.ReadImage(merge, Compute | Storage);
                REQUIRE(read.IsValid());
                
                RG::Resource write = graph.WriteImage(merge, Compute | Storage);
                REQUIRE(write.IsValid());

                passData.Resource = image;

                renderGraph.Compile(ctx);
                SUCCEED();
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){});
    }
    SECTION("Passes are topologically sorted")
    {
        TestGraphWatcher watcher;
        renderGraph.SetWatcher(watcher);
        
        struct Level0PassData
        {
            RG::Resource Resource{};
            RG::Resource ImageResource{};
        };
        struct Level1PassData
        {
            RG::Resource ResourceRead{};
            RG::Resource ImageResourceRead{};
            RG::Resource ResourceWrite{};
        };
        struct Level2PassData
        {
            RG::Resource Resource{};
        };
        auto& level0 = renderGraph.AddRenderPass<Level0PassData>("Level0"_hsv,
            [&](RG::Graph& graph, Level0PassData& passData)
            {
                passData.Resource = graph.Create("Buffer"_hsv, RG::RGBufferDescription{.SizeBytes = 4});
                passData.Resource = graph.WriteBuffer(passData.Resource, Compute | Storage);

                passData.ImageResource = graph.Create("Image"_hsv, RG::RGImageDescription{
                    .Width = 640,
                    .Height = 480,
                    .Format = Format::RGBA16_FLOAT});
                passData.ImageResource = graph.WriteImage(passData.ImageResource, Compute | Storage);
            },
            [&](const Level0PassData& passData, FrameContext& ctx, const RG::Graph& graph){});
        
        auto& level1 = renderGraph.AddRenderPass<Level1PassData>("Level1A"_hsv,
            [&](RG::Graph& graph, Level1PassData& passData)
            {
                passData.ResourceRead = graph.ReadBuffer(level0.Resource, Compute | Uniform);
                passData.ImageResourceRead = graph.ReadImage(level0.ImageResource, Compute | Sampled);

                passData.ResourceWrite = graph.Create("Buffer"_hsv, RG::RGBufferDescription{.SizeBytes = 4});
                passData.ResourceWrite = graph.WriteBuffer(passData.ResourceWrite, Compute | Storage);
            },
            [&](const Level1PassData& passData, FrameContext& ctx, const RG::Graph& graph){});
        
        renderGraph.AddRenderPass<Level2PassData>("Level2"_hsv,
            [&](RG::Graph& graph, Level2PassData& passData)
            {
                passData.Resource = graph.ReadBuffer(level1.ResourceWrite, Compute | Uniform);
            },
            [&](const Level2PassData& passData, FrameContext& ctx, const RG::Graph& graph){});
        
        renderGraph.AddRenderPass<Level1PassData>("Level1B"_hsv,
            [&](RG::Graph& graph, Level1PassData& passData)
            {
                passData.ResourceRead = graph.ReadBuffer(level0.Resource, Compute | Uniform);
            },
            [&](const Level1PassData& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.Compile(ctx);

        REQUIRE(watcher.Passes->front()->Name().AsString() == "Level0");
        REQUIRE(watcher.Passes->back()->Name().AsString() == "Level2");
    }
    SECTION("Simple buffer read-read does not result in barrier")
    {
        TestGraphWatcher watcher;
        renderGraph.SetWatcher(watcher);

        RG::Resource buffer = renderGraph.AddRenderPass<RG::Resource>("One"_hsv,
            [&](RG::Graph& graph, RG::Resource& passData)
            {
                passData = graph.Create("Buffer"_hsv, RG::RGBufferDescription{.SizeBytes = 4});
                passData = graph.ReadBuffer(passData, Compute | Storage);
            },
            [&](const RG::Resource& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.AddRenderPass<RG::Resource>("Two"_hsv,
            [&](RG::Graph& graph, RG::Resource& passData)
            {
                passData = graph.ReadBuffer(buffer, Compute | Storage);
            },
            [&](const RG::Resource& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.Compile(ctx);
        REQUIRE(watcher.Buffers->size() == 1);
        REQUIRE(watcher.BufferAccesses->size() == 2);
        REQUIRE(watcher.Barriers.empty());
    }
    SECTION("Simple image read-read does result only in one layout-transition barrier")
    {
        TestGraphWatcher watcher;
        renderGraph.SetWatcher(watcher);

        RG::Resource image = renderGraph.AddRenderPass<RG::Resource>("One"_hsv,
            [&](RG::Graph& graph, RG::Resource& passData)
            {
                passData = graph.Create("Image"_hsv, RG::RGImageDescription{
                    .Width = 640,
                    .Height = 480,
                    .Format = Format::RGBA16_UINT});
                passData = graph.ReadImage(passData, Compute | Storage);
            },
            [&](const RG::Resource& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.AddRenderPass<RG::Resource>("Two"_hsv,
            [&](RG::Graph& graph, RG::Resource& passData)
            {
                passData = graph.ReadImage(image, Compute | Storage);
            },
            [&](const RG::Resource& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.Compile(ctx);
        REQUIRE(watcher.Images->size() == 1);
        REQUIRE(watcher.ImageAccesses->size() == 2);
        REQUIRE(watcher.Barriers.size() == 1);
        REQUIRE(watcher.Barriers.front().BarrierType == RG::GraphWatcher::BarrierInfo::Type::Barrier);
        REQUIRE(watcher.Barriers.front().DependencyInfo.LayoutTransitionInfo.has_value());
        REQUIRE(watcher.Barriers.front().DependencyInfo.LayoutTransitionInfo.value().NewLayout
            == ImageLayout::General);
    }
    SECTION("Images have correct layout transition")
    {
        TestGraphWatcher watcher;
        renderGraph.SetWatcher(watcher);

        struct PassData
        {
            RG::Resource Depth32f;
            RG::Resource DepthStencil24;
            RG::Resource DepthStencil32;
            RG::Resource ComputeWrite;
        };

        renderGraph.AddRenderPass<PassData>("One"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                RG::RGImageDescription description{
                    .Width = 640,
                    .Height = 480,
                    .Format = Format::D32_FLOAT};
                passData.Depth32f = graph.Create("Depth32"_hsv, description);

                description.Format = Format::D24_UNORM_S8_UINT;
                passData.DepthStencil24 = graph.Create("DepthStencil24"_hsv, description);

                description.Format = Format::D32_FLOAT_S8_UINT;
                passData.DepthStencil32 = graph.Create("DepthStencil32"_hsv, description);
                passData.ComputeWrite = graph.Create("ComputeWrite"_hsv, description);

                passData.Depth32f = graph.ReadImage(passData.Depth32f, Compute | Storage);
                passData.DepthStencil24 = graph.ReadImage(passData.DepthStencil24, Compute | Storage);
                passData.DepthStencil32 = graph.ReadImage(passData.DepthStencil32, Compute | Storage);
                passData.ComputeWrite = graph.WriteImage(passData.ComputeWrite, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.Compile(ctx);
        REQUIRE(watcher.Barriers.size() == 4);
        REQUIRE(watcher.Barriers[0].DependencyInfo.LayoutTransitionInfo.has_value());
        REQUIRE(watcher.Barriers[0].DependencyInfo.LayoutTransitionInfo.value().NewLayout
            == ImageLayout::DepthReadonly);
        REQUIRE(watcher.Barriers[1].DependencyInfo.LayoutTransitionInfo.has_value());
        REQUIRE(watcher.Barriers[1].DependencyInfo.LayoutTransitionInfo.value().NewLayout
            == ImageLayout::DepthStencilReadonly);
        REQUIRE(watcher.Barriers[2].DependencyInfo.LayoutTransitionInfo.has_value());
        REQUIRE(watcher.Barriers[2].DependencyInfo.LayoutTransitionInfo.value().NewLayout
            == ImageLayout::DepthStencilReadonly);
        REQUIRE(watcher.Barriers[3].DependencyInfo.LayoutTransitionInfo.has_value());
        REQUIRE(watcher.Barriers[3].DependencyInfo.LayoutTransitionInfo.value().NewLayout == ImageLayout::General);
    }
    SECTION("Simple read-write results in execution barrier")
    {
        TestGraphWatcher watcher;
        renderGraph.SetWatcher(watcher);

        struct PassData
        {
            RG::Resource Buffer;
            RG::Resource Image;
        };

        PassData out = renderGraph.AddRenderPass<PassData>("One"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Buffer = graph.Create("Buffer"_hsv, RG::RGBufferDescription{.SizeBytes = 4});
                passData.Image = graph.Create("Image"_hsv, RG::RGImageDescription{
                    .Width = 640,
                    .Height = 480,
                    .Format = Format::RGBA16_UINT});
                passData.Buffer = graph.ReadBuffer(passData.Buffer, Compute | Storage);
                passData.Image = graph.ReadImage(passData.Image, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.AddRenderPass<PassData>("Two"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Buffer = graph.WriteBuffer(out.Buffer, Compute | Storage);
                passData.Image = graph.WriteImage(out.Image, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.Compile(ctx);
        REQUIRE(watcher.Barriers.size() == 3);
        bool hasLayoutTranstion = false;
        bool hasBufferExecutionDependency = false;
        bool hasImageWriteDependency = false;
        for (auto& barrier : watcher.Barriers)
        {
            REQUIRE(barrier.BarrierType == RG::GraphWatcher::BarrierInfo::Type::Barrier);
            
            if (barrier.DependencyInfo.LayoutTransitionInfo.has_value() &&
                barrier.DependencyInfo.LayoutTransitionInfo->SourceStage == PipelineStage::None)
                hasLayoutTranstion = true;
            if (barrier.DependencyInfo.MemoryDependencyInfo.has_value() &&
                barrier.DependencyInfo.MemoryDependencyInfo->SourceStage == PipelineStage::ComputeShader &&
                renderGraph.GetImage(barrier.Resource) == renderGraph.GetImage(out.Image))
                hasImageWriteDependency = true;
            else if (barrier.DependencyInfo.ExecutionDependencyInfo.has_value())
                if (barrier.Resource.IsBuffer() &&
                    renderGraph.GetBuffer(barrier.Resource) == renderGraph.GetBuffer(out.Buffer))
                    hasBufferExecutionDependency = true;
        }
        REQUIRE(watcher.Passes->front()->Name().AsStringView() == "One");
        REQUIRE(watcher.Barriers.size() == 3);
        REQUIRE(hasLayoutTranstion);
        REQUIRE(hasBufferExecutionDependency);
        REQUIRE(hasImageWriteDependency);
    }
    SECTION("Simple write-write results in memory barrier")
    {
        TestGraphWatcher watcher;
        renderGraph.SetWatcher(watcher);

        struct PassData
        {
            RG::Resource Buffer;
            RG::Resource Image;
        };

        PassData out = renderGraph.AddRenderPass<PassData>("One"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Buffer = graph.Create("Buffer"_hsv, RG::RGBufferDescription{.SizeBytes = 4});
                passData.Image = graph.Create("Image"_hsv, RG::RGImageDescription{
                    .Width = 640,
                    .Height = 480,
                    .Format = Format::RGBA16_UINT});
                passData.Buffer = graph.WriteBuffer(passData.Buffer, Compute | Storage);
                passData.Image = graph.WriteImage(passData.Image, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.AddRenderPass<PassData>("Two"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Buffer = graph.WriteBuffer(out.Buffer, Compute | Storage);
                passData.Image = graph.WriteImage(out.Image, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.Compile(ctx);
        REQUIRE(watcher.Barriers.size() == 3);
        bool hasLayoutTranstion = false;
        bool hasBufferMemoryDependency = false;
        bool hasImageMemoryDependency = false;
        for (auto& barrier : watcher.Barriers)
        {
            REQUIRE(barrier.BarrierType == RG::GraphWatcher::BarrierInfo::Type::Barrier);
            
            if (barrier.DependencyInfo.LayoutTransitionInfo.has_value() &&
                barrier.DependencyInfo.LayoutTransitionInfo->SourceStage == PipelineStage::None)
                hasLayoutTranstion = true;
            else if (barrier.DependencyInfo.MemoryDependencyInfo.has_value())
            {
                if (barrier.Resource.IsBuffer() &&
                    renderGraph.GetBuffer(barrier.Resource) == renderGraph.GetBuffer(out.Buffer))
                    hasBufferMemoryDependency = true;
                else if (barrier.Resource.IsImage() &&
                    renderGraph.GetImage(barrier.Resource) == renderGraph.GetImage(out.Image))
                    hasImageMemoryDependency = true;
            }
        }
        REQUIRE(watcher.Barriers.size() == 3);
        REQUIRE(hasLayoutTranstion);
        REQUIRE(hasBufferMemoryDependency);
        REQUIRE(hasImageMemoryDependency);
    }
    SECTION("Simple write-read results in memory barrier")
    {
        TestGraphWatcher watcher;
        renderGraph.SetWatcher(watcher);

        struct PassData
        {
            RG::Resource Buffer;
            RG::Resource Image;
        };

        PassData out = renderGraph.AddRenderPass<PassData>("One"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Buffer = graph.Create("Buffer"_hsv, RG::RGBufferDescription{.SizeBytes = 4});
                passData.Image = graph.Create("Image"_hsv, RG::RGImageDescription{
                    .Width = 640,
                    .Height = 480,
                    .Format = Format::RGBA16_UINT});
                passData.Buffer = graph.WriteBuffer(passData.Buffer, Compute | Storage);
                passData.Image = graph.WriteImage(passData.Image, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.AddRenderPass<PassData>("Two"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Buffer = graph.ReadBuffer(out.Buffer, Compute | Storage);
                passData.Image = graph.ReadImage(out.Image, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.Compile(ctx);
        REQUIRE(watcher.Barriers.size() == 3);
        bool hasLayoutTranstion = false;
        bool hasBufferMemoryDependency = false;
        bool hasImageWriteDependency = false;
        for (auto& barrier : watcher.Barriers)
        {
            REQUIRE(barrier.BarrierType == RG::GraphWatcher::BarrierInfo::Type::Barrier);
            
            if (barrier.DependencyInfo.LayoutTransitionInfo.has_value() &&
                barrier.DependencyInfo.LayoutTransitionInfo->SourceStage == PipelineStage::None)
                hasLayoutTranstion = true;
            if (barrier.DependencyInfo.MemoryDependencyInfo.has_value() &&
                barrier.DependencyInfo.MemoryDependencyInfo->SourceStage == PipelineStage::ComputeShader &&
                barrier.Resource.IsImage() &&
                renderGraph.GetImage(barrier.Resource) == renderGraph.GetImage(out.Image))
                hasImageWriteDependency = true;
            else if (barrier.DependencyInfo.MemoryDependencyInfo.has_value() &&
                barrier.Resource.IsBuffer() &&
                renderGraph.GetBuffer(barrier.Resource) == renderGraph.GetBuffer(out.Buffer))
                hasBufferMemoryDependency = true;
        }
        REQUIRE(watcher.Barriers.size() == 3);
        REQUIRE(hasLayoutTranstion);
        REQUIRE(hasBufferMemoryDependency);
        REQUIRE(hasImageWriteDependency);
    }
    SECTION("Split barriers")
    {
        TestGraphWatcher watcher;
        renderGraph.SetWatcher(watcher);

        struct PassData
        {
            RG::Resource Buffer;
            RG::Resource Image;
        };

        PassData producer = renderGraph.AddRenderPass<PassData>("Producer"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Buffer = graph.Create("Buffer"_hsv, RG::RGBufferDescription{.SizeBytes = 4});
                passData.Image = graph.Create("Image"_hsv, RG::RGImageDescription{
                    .Width = 640,
                    .Height = 480,
                    .Format = Format::RGBA16_UINT});
                passData.Buffer = graph.WriteBuffer(passData.Buffer, Compute | Storage);
                passData.Image = graph.WriteImage(passData.Image, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){});
        PassData consumer = renderGraph.AddRenderPass<PassData>("Consumer"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Buffer = graph.ReadBuffer(producer.Buffer, Compute | Storage);
                passData.Image = graph.ReadImage(producer.Image, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){});

        PassData producer2 = renderGraph.AddRenderPass<PassData>("Producer2"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Buffer = graph.Create("Buffer"_hsv, RG::RGBufferDescription{.SizeBytes = 4});
                passData.Image = graph.Create("Image"_hsv, RG::RGImageDescription{
                    .Width = 640,
                    .Height = 480,
                    .Format = Format::RGBA16_UINT});
                passData.Buffer = graph.WriteBuffer(passData.Buffer, Compute | Storage);
                passData.Image = graph.WriteImage(passData.Image, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){});
        PassData consumer2 = renderGraph.AddRenderPass<PassData>("Consumer2"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Buffer = graph.ReadBuffer(producer2.Buffer, Compute | Storage);
                passData.Image = graph.ReadImage(producer2.Image, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){});

        PassData producer3 = renderGraph.AddRenderPass<PassData>("Producer3"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Buffer = graph.WriteBuffer(producer.Buffer, Compute | Storage);
                passData.Image = graph.WriteImage(producer.Image, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){});
        
        renderGraph.Compile(ctx);
        /* two image barriers and 3 split-barriers for buffers and images */
        REQUIRE(watcher.Barriers.size() == 2 + 3 + 3);
        for (auto& barrier : watcher.Barriers)
        {
            /* of all barriers there should be only two which are not split barriers
             * and they should be image transitions from undefined */
            if (barrier.BarrierType == RG::GraphWatcher::BarrierInfo::Type::Barrier &&
                (!barrier.DependencyInfo.LayoutTransitionInfo.has_value() ||
                barrier.DependencyInfo.LayoutTransitionInfo->OldLayout != ImageLayout::Undefined))
                REQUIRE(false);
        }
        SUCCEED();
    }
    SECTION("All split-merge resource accesses are accounted for")
    {
        TestGraphWatcher watcher;
        renderGraph.SetWatcher(watcher);

        struct PassData
        {
            RG::Resource Resource{};
        };
        RG::Resource image = renderGraph.AddRenderPass<PassData>("CreateImage"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                RG::Resource image = graph.Create("MyImage"_hsv, RG::RGImageDescription{
                    .Width = 640,
                    .Height = 480,
                    .Mipmaps = 3,
                    .Format = Format::RGBA16_FLOAT});
                passData.Resource = graph.WriteImage(image, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){}).Resource;

        RG::Resource split0 = renderGraph.SplitImage(image, {.MipmapBase = 1});
        RG::Resource split1 = renderGraph.SplitImage(image, {.MipmapBase = 2});

        split0 = renderGraph.AddRenderPass<PassData>("WriteToSplit0"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Resource = graph.WriteImage(split0, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){}).Resource;
        split0 = renderGraph.AddRenderPass<PassData>("ReadSplit0"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Resource = graph.ReadImage(split0, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){}).Resource;
        split1 = renderGraph.AddRenderPass<PassData>("WriteToSplit1"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Resource = graph.WriteImage(split1, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){}).Resource;

        image = renderGraph.MergeImage({split0, split1});
        
        image = renderGraph.AddRenderPass<PassData>("ReadMerged"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Resource = graph.ReadImage(image, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){}).Resource;
        image = renderGraph.AddRenderPass<PassData>("WriteMerged"_hsv,
            [&](RG::Graph& graph, PassData& passData)
            {
                passData.Resource = graph.WriteImage(image, Compute | Storage);
            },
            [&](const PassData& passData, FrameContext& ctx, const RG::Graph& graph){}).Resource;

        renderGraph.Compile(ctx);
        /* here we do not check barriers themselves, but the order of passes */
        REQUIRE(watcher.Passes->at(0)->Name().AsString() == "CreateImage");
        REQUIRE(watcher.Passes->at(1)->Name().AsString() == "Split");
        REQUIRE(watcher.Passes->at(2)->Name().AsString() == "Split");
        REQUIRE(watcher.Passes->at(3)->Name().AsString() == "WriteToSplit0");
        REQUIRE(watcher.Passes->at(4)->Name().AsString() == "WriteToSplit1");
        REQUIRE(watcher.Passes->at(5)->Name().AsString() == "ReadSplit0");
        REQUIRE(watcher.Passes->at(6)->Name().AsString() == "Merge");
        REQUIRE(watcher.Passes->at(7)->Name().AsString() == "ReadMerged");
        REQUIRE(watcher.Passes->at(8)->Name().AsString() == "WriteMerged");
    }
    SECTION("Can access image as render target")
    {
        TestGraphWatcher watcher;
        renderGraph.SetWatcher(watcher);

        RG::Resource color = renderGraph.Create("Color"_hsv, RG::RGImageDescription{
            .Width = 640,
            .Height = 480,
            .Format = Format::RGBA16_FLOAT
        });
        RG::Resource depth = renderGraph.Create("Depth"_hsv, RG::RGImageDescription{
            .Width = 640,
            .Height = 480,
            .Format = Format::D32_FLOAT
        });
        
        renderGraph.AddRenderPass<u32>("RenderPass"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                graph.RenderTarget(color, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
                graph.DepthStencilTarget(depth, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.Compile(ctx);
        REQUIRE(watcher.ImageAccesses->size() == 2);
        REQUIRE(watcher.ImageAccesses->front().Access == PipelineAccess::WriteColorAttachment);
        REQUIRE(watcher.ImageAccesses->back().Access == PipelineAccess::WriteDepthStencilAttachment);
    }
    SECTION("Can do multipass render using auto-update resources")
    {
        TestGraphWatcher watcher;
        renderGraph.SetWatcher(watcher);

        RG::Resource color = renderGraph.Create("Color"_hsv, RG::ResourceCreationFlags::AutoUpdate,
            RG::RGImageDescription{
                .Width = 640,
                .Height = 480,
                .Format = Format::RGBA16_FLOAT
        });
        RG::Resource depth = renderGraph.Create("Depth"_hsv, RG::ResourceCreationFlags::AutoUpdate,
            RG::RGImageDescription{
                .Width = 640,
                .Height = 480,
                .Format = Format::D32_FLOAT
        });

        /* note that color and depth resources are not updated */
        renderGraph.AddRenderPass<u32>("RenderCull"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                graph.RenderTarget(color, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
                graph.DepthStencilTarget(depth, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.AddRenderPass<u32>("RenderReocclusion"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                graph.RenderTarget(color, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
                graph.DepthStencilTarget(depth, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.Compile(ctx);
        REQUIRE(watcher.Barriers.size() == 4);
        REQUIRE(renderGraph.GetImage(watcher.Barriers[0].Resource) == renderGraph.GetImage(color));
        REQUIRE(renderGraph.GetImage(watcher.Barriers[1].Resource) == renderGraph.GetImage(depth));
        REQUIRE(renderGraph.GetImage(watcher.Barriers[2].Resource) == renderGraph.GetImage(color));
        REQUIRE(renderGraph.GetImage(watcher.Barriers[3].Resource) == renderGraph.GetImage(depth));
        REQUIRE(watcher.Barriers[0].DependencyInfo.LayoutTransitionInfo.has_value());
        REQUIRE(watcher.Barriers[1].DependencyInfo.LayoutTransitionInfo.has_value());
        REQUIRE(watcher.Barriers[2].DependencyInfo.MemoryDependencyInfo.has_value());
        REQUIRE(watcher.Barriers[3].DependencyInfo.MemoryDependencyInfo.has_value());
    }
    SECTION("Can do multipass render using auto-update resources with access in-between")
    {
        TestGraphWatcher watcher;
        renderGraph.SetWatcher(watcher);

        RG::Resource color = renderGraph.Create("Color"_hsv, RG::ResourceCreationFlags::AutoUpdate,
            RG::RGImageDescription{
                .Width = 640,
                .Height = 480,
                .Format = Format::RGBA16_FLOAT
        });
        RG::Resource depth = renderGraph.Create("Depth"_hsv, RG::ResourceCreationFlags::AutoUpdate,
            RG::RGImageDescription{
                .Width = 640,
                .Height = 480,
                .Format = Format::D32_FLOAT
        });

        /* note that color and depth resources are not updated */
        renderGraph.AddRenderPass<u32>("RenderCull"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                graph.RenderTarget(color, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
                graph.DepthStencilTarget(depth, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.AddRenderPass<u32>("ReadColorForReasons"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                graph.ReadImage(color, Compute | Storage);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.AddRenderPass<u32>("RenderReocclusion"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                graph.RenderTarget(color, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
                graph.DepthStencilTarget(depth, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.Compile(ctx);
        REQUIRE(watcher.Passes->at(0)->Name().AsStringView() == "RenderCull");
        REQUIRE(watcher.Passes->at(1)->Name().AsStringView() == "ReadColorForReasons");
        REQUIRE(watcher.Passes->at(2)->Name().AsStringView() == "RenderReocclusion");

        /* 2 transitions from undefined + transtion from color to readonly + transition from readonly to color +
         * split-barrier on depth
         */
        REQUIRE(watcher.Barriers.size() == 5);
        REQUIRE(watcher.Barriers[0].DependencyInfo.LayoutTransitionInfo.has_value());
        REQUIRE(watcher.Barriers[1].DependencyInfo.LayoutTransitionInfo.has_value());
        REQUIRE((watcher.Barriers[2].DependencyInfo.LayoutTransitionInfo.has_value() &&
            watcher.Barriers[2].SecondPass == watcher.Passes->at(1).get()));
        REQUIRE((watcher.Barriers[3].DependencyInfo.LayoutTransitionInfo.has_value() &&
            watcher.Barriers[3].SecondPass == watcher.Passes->at(2).get()));
        REQUIRE((watcher.Barriers[4].BarrierType == RG::GraphWatcher::BarrierInfo::Type::SplitBarrier &&
            watcher.Barriers[4].DependencyInfo.MemoryDependencyInfo.has_value() &&
            watcher.Barriers[4].FirstPass == watcher.Passes->at(0).get() &&
            watcher.Barriers[4].SecondPass == watcher.Passes->at(2).get()));
    }
    SECTION("Can do multipass render using auto-update resources to splits")
    {
        TestGraphWatcher watcher;
        renderGraph.SetWatcher(watcher);

        RG::Resource color = renderGraph.Create("Color"_hsv, RG::ResourceCreationFlags::AutoUpdate,
            RG::RGImageDescription{
                .Width = 640,
                .Height = 480,
                .Format = Format::RGBA16_FLOAT
        });
        RG::Resource depth = renderGraph.Create("Depth"_hsv, RG::ResourceCreationFlags::AutoUpdate,
            RG::RGImageDescription{
                .Width = 640,
                .Height = 480,
                .Format = Format::D32_FLOAT
        });

        RG::Resource colorSplit0 = renderGraph.SplitImage(color, {.MipmapBase = 0, .Mipmaps = 1});
        RG::Resource colorSplit1 = renderGraph.SplitImage(color, {.MipmapBase = 1, .Mipmaps = 1});
        
        RG::Resource depthSplit0 = renderGraph.SplitImage(depth, {.MipmapBase = 0, .Mipmaps = 1});
        RG::Resource depthSplit1 = renderGraph.SplitImage(depth, {.MipmapBase = 1, .Mipmaps = 1});

        /* note that color and depth resources are not updated */
        renderGraph.AddRenderPass<u32>("RenderCull0"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                graph.RenderTarget(colorSplit0, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
                graph.DepthStencilTarget(depthSplit0, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.AddRenderPass<u32>("RenderCull1"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                graph.RenderTarget(colorSplit1, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
                graph.DepthStencilTarget(depthSplit1, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.AddRenderPass<u32>("RenderReocclusion0"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                graph.RenderTarget(colorSplit0, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
                graph.DepthStencilTarget(depthSplit0, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.AddRenderPass<u32>("RenderReocclusion1"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                graph.RenderTarget(colorSplit1, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
                graph.DepthStencilTarget(depthSplit1, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.AddRenderPass<u32>("RenderReocclusion1Extra"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                graph.RenderTarget(colorSplit1, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
                graph.DepthStencilTarget(depthSplit1, {
                    .OnLoad = AttachmentLoad::Clear,
                    .OnStore = AttachmentStore::Store
                });
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});

        color = renderGraph.MergeImage({colorSplit0, colorSplit1});
        renderGraph.AddRenderPass<u32>("CustomMerge"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                depth = renderGraph.MergeImage({depthSplit0, depthSplit1});
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.AddRenderPass<u32>("ReadMerged"_hsv,
            [&](RG::Graph& graph, u32& passData)
            {
                graph.ReadImage(color, Compute | Sampled);
                graph.ReadImage(depth, Compute | Sampled);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        
        renderGraph.Compile(ctx);
        /* 4 transitions from undefined +
         * 2 memory barriers for each resource * 3 +
         * split-barrier on color +
         * split-barrier on depth +
         */
        REQUIRE(watcher.Barriers.size() == 12);
        REQUIRE((watcher.Barriers[0].DependencyInfo.LayoutTransitionInfo.has_value() &&
            renderGraph.GetImage(watcher.Barriers[0].Resource) == renderGraph.GetImage(color)));
        REQUIRE((watcher.Barriers[1].DependencyInfo.LayoutTransitionInfo.has_value() &&
            renderGraph.GetImage(watcher.Barriers[1].Resource) == renderGraph.GetImage(depth)));
        REQUIRE((watcher.Barriers[2].DependencyInfo.LayoutTransitionInfo.has_value() &&
            renderGraph.GetImage(watcher.Barriers[2].Resource) == renderGraph.GetImage(color)));
        REQUIRE((watcher.Barriers[3].DependencyInfo.LayoutTransitionInfo.has_value() &&
            renderGraph.GetImage(watcher.Barriers[3].Resource) == renderGraph.GetImage(depth)));
        for (u32 i = 4; i < 2 + 2 * 4; i++)
            REQUIRE((watcher.Barriers[i].DependencyInfo.MemoryDependencyInfo.has_value() &&
                renderGraph.GetImage(watcher.Barriers[i].Resource) == renderGraph.GetImage(i % 2 == 0 ? color : depth)));
        REQUIRE(watcher.Barriers[10].BarrierType == RG::GraphWatcher::BarrierInfo::Type::SplitBarrier); 
        REQUIRE(watcher.Barriers[11].BarrierType == RG::GraphWatcher::BarrierInfo::Type::SplitBarrier);
    }
    SECTION("Resources can be aliased")
    {
        RG::Resource buffer0 = renderGraph.Create("Buffer"_hsv, RG::RGBufferDescription{
            .SizeBytes = 4});
        RG::Resource buffer1 = renderGraph.Create("BufferAliased"_hsv, RG::RGBufferDescription{
            .SizeBytes = 4});
        
        RG::Resource depth0 = renderGraph.Create("Depth"_hsv, RG::RGImageDescription{
            .Width = 640,
            .Height = 480,
            .Format = Format::D32_FLOAT
        });
        RG::Resource depth1 = renderGraph.Create("DepthAliased"_hsv, RG::RGImageDescription{
            .Width = 640,
            .Height = 480,
            .Format = Format::D32_FLOAT
        });
        renderGraph.AddRenderPass<u32>("BufferAccess0"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                buffer0 = graph.WriteBuffer(buffer0, Compute | Storage);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.AddRenderPass<u32>("BufferAccess1"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                buffer1 = graph.WriteBuffer(buffer1, Compute | Storage);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        
        
        renderGraph.AddRenderPass<u32>("ImageAccess0"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                depth0 = graph.WriteImage(depth0, Compute | Storage);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.AddRenderPass<u32>("ImageAccess1"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                depth1 = graph.WriteImage(depth1, Compute | Storage);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.Compile(ctx);
        REQUIRE(renderGraph.GetBuffer(buffer0) == renderGraph.GetBuffer(buffer1));
        REQUIRE(renderGraph.GetImage(depth0) == renderGraph.GetImage(depth1));
    }
    
    SECTION("Aliased resources inherit last pipeline stage and access")
    {
        TestGraphWatcher watcher;
        renderGraph.SetWatcher(watcher);

        RG::Resource buffer0 = renderGraph.Create("Buffer"_hsv, RG::RGBufferDescription{
            .SizeBytes = 4});
        RG::Resource buffer1 = renderGraph.Create("BufferAliased"_hsv, RG::RGBufferDescription{
            .SizeBytes = 4});
        
        RG::Resource depth0 = renderGraph.Create("Depth"_hsv, RG::RGImageDescription{
            .Width = 640,
            .Height = 480,
            .Format = Format::D32_FLOAT
        });
        RG::Resource depth1 = renderGraph.Create("DepthAliased"_hsv, RG::RGImageDescription{
            .Width = 640,
            .Height = 480,
            .Format = Format::D32_FLOAT
        });
        renderGraph.AddRenderPass<u32>("BufferAccess0"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                buffer0 = graph.WriteBuffer(buffer0, Compute | Storage);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.AddRenderPass<u32>("BufferAccess1"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                buffer1 = graph.WriteBuffer(buffer1, Compute | Storage);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        
        
        renderGraph.AddRenderPass<u32>("ImageAccess0"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                depth0 = graph.WriteImage(depth0, Compute | Storage);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.AddRenderPass<u32>("ImageAccess1"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                depth1 = graph.WriteImage(depth1, Compute | Storage);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.Compile(ctx);
        /* 1 for buffer and 2 for images */
        REQUIRE(watcher.Barriers.size() == 3);
        REQUIRE(watcher.Barriers[0].Resource == buffer1);
        REQUIRE(watcher.Barriers[0].DependencyInfo.MemoryDependencyInfo->SourceStage == PipelineStage::ComputeShader);
        REQUIRE(watcher.Barriers[0].DependencyInfo.MemoryDependencyInfo->SourceAccess == PipelineAccess::WriteShader);
        REQUIRE(watcher.Barriers[1].Resource == depth0);
        REQUIRE(watcher.Barriers[2].Resource == depth1);
        REQUIRE(watcher.Barriers[2].DependencyInfo.LayoutTransitionInfo->SourceStage == PipelineStage::ComputeShader);
        REQUIRE(watcher.Barriers[2].DependencyInfo.LayoutTransitionInfo->SourceAccess == PipelineAccess::WriteShader);
        /* the old content should always be discarded */
        REQUIRE(watcher.Barriers[2].DependencyInfo.LayoutTransitionInfo->OldLayout == ImageLayout::Undefined);
    }
    SECTION("Readback resources can be aliased only after frames in flight span")
    {
        RG::Resource buffer = {};
        renderGraph.AddRenderPass<u32>("BufferAccess0"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                buffer = renderGraph.Create("Buffer"_hsv, RG::RGBufferDescription{
                    .SizeBytes = 4});
                buffer = graph.ReadBuffer(buffer, Compute | Storage | Readback);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.Compile(ctx);
        Buffer physicallBuffer0 = renderGraph.GetBuffer(buffer);

        renderGraph.Reset();
        renderGraph.AddRenderPass<u32>("BufferAccess1"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                buffer = renderGraph.Create("Buffer"_hsv, RG::RGBufferDescription{
                    .SizeBytes = 4});
                buffer = graph.ReadBuffer(buffer, Compute | Storage | Readback);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.Compile(ctx);
        Buffer physicallBuffer1 = renderGraph.GetBuffer(buffer);

        REQUIRE(physicallBuffer0 != physicallBuffer1);

        renderGraph.Reset();
        renderGraph.AddRenderPass<u32>("BufferAccess2"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                buffer = renderGraph.Create("Buffer"_hsv, RG::RGBufferDescription{
                    .SizeBytes = 4});
                buffer = graph.ReadBuffer(buffer, Compute | Storage | Readback);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.Compile(ctx);
        Buffer physicallBuffer2 = renderGraph.GetBuffer(buffer);

        REQUIRE(physicallBuffer0 == physicallBuffer2);
    }
    SECTION("Resources with different Usage types are not aliased")
    {
        RG::Resource buffer0 = renderGraph.Create("Buffer"_hsv, RG::RGBufferDescription{
            .SizeBytes = 4});
        RG::Resource buffer1 = renderGraph.Create("BufferNotAliased"_hsv, RG::RGBufferDescription{
            .SizeBytes = 4});
        
        RG::Resource depth0 = renderGraph.Create("Depth"_hsv, RG::RGImageDescription{
            .Width = 640,
            .Height = 480,
            .Format = Format::D32_FLOAT
        });
        RG::Resource depth1 = renderGraph.Create("DepthNotAliased"_hsv, RG::RGImageDescription{
            .Width = 640,
            .Height = 480,
            .Format = Format::D32_FLOAT
        });
        renderGraph.AddRenderPass<u32>("BufferAccess0"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                buffer0 = graph.WriteBuffer(buffer0, Compute | Storage);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.AddRenderPass<u32>("BufferAccess1"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                buffer1 = graph.WriteBuffer(buffer1, Compute | Storage | Conditional);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        
        
        renderGraph.AddRenderPass<u32>("ImageAccess0"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                depth0 = graph.WriteImage(depth0, Compute | Storage);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.AddRenderPass<u32>("ImageAccess1"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                depth1 = graph.WriteImage(depth1, Compute | Storage | Sampled);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.Compile(ctx);
        REQUIRE(renderGraph.GetBuffer(buffer0) != renderGraph.GetBuffer(buffer1));
        REQUIRE(renderGraph.GetImage(depth0) != renderGraph.GetImage(depth1));
    }
    SECTION("Imported resources are not aliased")
    {
        Buffer physicalBuffer = Device::CreateBuffer({
            .SizeBytes = 4,
            .Usage = BufferUsage::Storage});
        RG::Resource buffer0 = renderGraph.Import("Buffer"_hsv, physicalBuffer);
        RG::Resource buffer1 = renderGraph.Create("BufferAliased"_hsv, RG::RGBufferDescription{
            .SizeBytes = 4});

        Image physicalDepth = Device::CreateImage({
            .Description = ImageDescription{
                .Width = 640,
                .Height = 480,
                .Format = Format::D32_FLOAT,
                .Usage = ImageUsage::Storage}});
        RG::Resource depth0 = renderGraph.Import("Depth"_hsv, physicalDepth);
        RG::Resource depth1 = renderGraph.Create("DepthAliased"_hsv, RG::RGImageDescription{
            .Width = 640,
            .Height = 480,
            .Format = Format::D32_FLOAT
        });
        renderGraph.AddRenderPass<u32>("BufferAccess0"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                buffer0 = graph.WriteBuffer(buffer0, Compute | Storage);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.AddRenderPass<u32>("BufferAccess1"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                buffer1 = graph.WriteBuffer(buffer1, Compute | Storage);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        
        
        renderGraph.AddRenderPass<u32>("ImageAccess0"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                depth0 = graph.WriteImage(depth0, Compute | Storage);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});
        renderGraph.AddRenderPass<u32>("ImageAccess1"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                depth1 = graph.WriteImage(depth1, Compute | Storage);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});

        renderGraph.Compile(ctx);
        REQUIRE(renderGraph.GetBuffer(buffer0) != renderGraph.GetBuffer(buffer1));
        REQUIRE(renderGraph.GetImage(depth0) != renderGraph.GetImage(depth1));
    }
    SECTION("Can export resources")
    {
        RG::Resource buffer = renderGraph.Create("Buffer"_hsv, RG::RGBufferDescription{
           .SizeBytes = 4});
        
        RG::Resource depth = renderGraph.Create("Depth"_hsv, RG::RGImageDescription{
            .Width = 640,
            .Height = 480,
            .Format = Format::D32_FLOAT
        });
        renderGraph.AddRenderPass<u32>("ExportPass"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                buffer = graph.ReadBuffer(buffer, Compute | Storage);
                depth = graph.ReadImage(depth, Compute | Storage);
                graph.MarkBufferForExport(buffer);
                graph.MarkImageForExport(depth);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});

        

        REQUIRE(!renderGraph.GetBuffer(buffer).HasValue());
        REQUIRE(!renderGraph.GetImage(depth).HasValue());

        Buffer physicalBuffer;
        Image physicalDepth;
        renderGraph.ClaimBuffer(buffer, physicalBuffer, ctx.DeletionQueue);
        renderGraph.ClaimImage(depth, physicalDepth, ctx.DeletionQueue);
        REQUIRE(renderGraph.GetBuffer(buffer).HasValue());
        REQUIRE(renderGraph.GetBuffer(buffer) == physicalBuffer);
        REQUIRE(renderGraph.GetImage(depth).HasValue());
        REQUIRE(renderGraph.GetImage(depth) == physicalDepth);

        Buffer bufferCopy = physicalBuffer;
        Image depthCopy = physicalDepth;
        renderGraph.ClaimBuffer(buffer, physicalBuffer, ctx.DeletionQueue);
        renderGraph.ClaimImage(depth, physicalDepth, ctx.DeletionQueue);
        REQUIRE(renderGraph.GetBuffer(buffer) == physicalBuffer);
        REQUIRE(bufferCopy == physicalBuffer);
        REQUIRE(renderGraph.GetImage(depth) == physicalDepth);
        REQUIRE(depthCopy == physicalDepth);
    }
    
    SECTION("Unclaimed exported resources are allocated on execution")
    {
        RG::Resource buffer = renderGraph.Create("Buffer"_hsv, RG::RGBufferDescription{
           .SizeBytes = 4});
        
        RG::Resource depth = renderGraph.Create("Depth"_hsv, RG::RGImageDescription{
            .Width = 640,
            .Height = 480,
            .Format = Format::D32_FLOAT
        });
        renderGraph.AddRenderPass<u32>("ExportPass"_hsv, [&](RG::Graph& graph, u32& passData)
            {
                buffer = graph.ReadBuffer(buffer, Compute | Storage);
                depth = graph.ReadImage(depth, Compute | Storage);
                graph.MarkBufferForExport(buffer);
                graph.MarkImageForExport(depth);
            },
            [&](const u32& passData, FrameContext& ctx, const RG::Graph& graph){});

        
        REQUIRE(!renderGraph.GetBuffer(buffer).HasValue());
        REQUIRE(!renderGraph.GetImage(depth).HasValue());

        renderGraph.Compile(ctx);

        REQUIRE(renderGraph.GetBuffer(buffer).HasValue());
        REQUIRE(renderGraph.GetImage(depth).HasValue());
    }
}

// NOLINTEND