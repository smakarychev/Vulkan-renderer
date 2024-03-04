#include "Renderer.h"
#include "GLFW/glfw3.h"
#include "RenderGraph\RenderGraphBlackboard.h"
#include "RenderGraph\RenderGraph.h"
#include "utils\utils.h"

void graphTest()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // do not create opengl context
    auto window = glfwCreateWindow(1600, 900, "My window", nullptr, nullptr);

    auto device = Device::Builder()
        .Defaults()
        .SetWindow(window)
        .AsyncCompute()
        .Build();

    Driver::Init(device);

    {
        RenderGraph::Graph renderGraph = {};
        RenderGraph::Blackboard blackboard = {};

        struct VisibilityData
        {
            RenderGraph::Resource DepthPyramid;
            RenderGraph::Resource VisibilityOut;
            RenderGraph::Resource BufferOut;
        };
        struct VisibilityDrawData
        {
            RenderGraph::Resource VisibilityIn;
            RenderGraph::Resource ColorOut;
            RenderGraph::Resource BufferIn;
        };
        
        {
            // generate visibility buffer

            RenderGraph::Pass& visibilityPass = renderGraph.AddRenderPass<VisibilityData>("visibility-pass",
                [&](RenderGraph::Graph& graph, VisibilityData& passData)
                {
                    passData.DepthPyramid = graph.CreateResource("depth-pyramid", RenderGraph::GraphTextureDescription{
                        .Width = 1024,
                        .Height = 512,
                        .Format = Format::R32_FLOAT,
                        .Kind = ImageKind::Image2d});

                    passData.VisibilityOut = graph.CreateResource("visibility-buffer", RenderGraph::GraphTextureDescription{
                        .Width = 1600,
                        .Height = 900,
                        .Format = Format::R32_UINT,
                        .Kind = ImageKind::Image2d,
                        .MipmapFilter = ImageFilter::Nearest});

                    passData.BufferOut = graph.CreateResource("buffer-out", RenderGraph::GraphBufferDescription{
                        1024});
                    
                    passData.DepthPyramid = graph.Read(passData.DepthPyramid,
                        RenderGraph::ResourceAccessFlags::Compute | RenderGraph::ResourceAccessFlags::Sampled);

                    passData.VisibilityOut = graph.RenderTarget(passData.VisibilityOut,
                        AttachmentLoad::Clear, AttachmentStore::Store, std::bit_cast<glm::vec4>(glm::uvec4(~0u, 0u, 0u, 0u)));

                    passData.BufferOut = graph.Write(passData.BufferOut,
                        RenderGraph::ResourceAccessFlags::Compute | RenderGraph::ResourceAccessFlags::Storage);

                    blackboard.RegisterOutput(passData);
                },
                [=](VisibilityData& passData, FrameContext& frameContext, const RenderGraph::Resources& resources)
                {});
        }
        
        {
            // visualize visibility buffer
            RenderGraph::Pass& visibilityDrawPass = renderGraph.AddRenderPass<VisibilityDrawData>("visibility-render-pass",
               [&](RenderGraph::Graph& graph, VisibilityDrawData& passData)
               {
                   const VisibilityData& visibilityData = blackboard.GetOutput<VisibilityData>();
                   
                   passData.ColorOut = graph.CreateResource("color-output", RenderGraph::GraphTextureDescription{
                       .Width = 1600,
                       .Height = 900,
                       .Format = Format::RGBA16_FLOAT,
                       .Kind = ImageKind::Image2d});

                   passData.VisibilityIn = graph.Read(visibilityData.VisibilityOut,
                       RenderGraph::ResourceAccessFlags::Pixel | RenderGraph::ResourceAccessFlags::Sampled);
                   passData.ColorOut = graph.RenderTarget(passData.ColorOut,
                       AttachmentLoad::Unspecified, AttachmentStore::Store);
                   passData.BufferIn = graph.Read(visibilityData.BufferOut,
                       RenderGraph::ResourceAccessFlags::Pixel | RenderGraph::ResourceAccessFlags::Storage);

                   blackboard.RegisterOutput(passData);
               },
               [=](VisibilityDrawData& passData, FrameContext& frameContext, const RenderGraph::Resources& resources)
               {});
        }
        
        renderGraph.Compile();
        std::cout << renderGraph.MermaidDump() << "\n";
    }
    

    Driver::Shutdown();
}

i32 main()
{
    utils::runSubProcess("../tools/AssetConverter/bin/Release-windows-x86_64/AssetConverter/AssetConverter.exe", {"../assets"});

    //graphTest();
    
    Renderer* renderer = Renderer::Get();
    renderer->Init();
    renderer->Run();
}
