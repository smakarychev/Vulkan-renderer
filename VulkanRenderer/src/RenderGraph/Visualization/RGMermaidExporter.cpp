#include "rendererpch.h"

#include "RGMermaidExporter.h"

#include "RenderGraph/RGPass.h"
#include "RenderGraph/RGResource.h"

namespace RG
{
    void RGMermaidExporter::OnPassOrderFinalized(const std::vector<std::unique_ptr<Pass>>& passes)
    {
        for (auto& pass : passes)
            m_Stream << std::format("\tpass.\"{}\"[/Pass {}/]\n", pass->Name().Hash(), pass->Name());
    }

    void RGMermaidExporter::OnBufferResourcesFinalized(const std::vector<BufferResource>& buffers)
    {
        for (u32 bufferIndex = 0; bufferIndex < buffers.size(); bufferIndex++)
        {
            auto& buffer = buffers.at(bufferIndex);
            m_BufferIndexToDescription[bufferIndex] = std::format("\tbuffer.{}[\"{}\n\t{}\n\t{}\"]\n",
                bufferIndex, buffer.Name,
                buffer.Description.SizeBytes,
                BufferTraits::bufferUsageToString(buffer.Description.Usage));
        }
    }

    void RGMermaidExporter::OnImageResourcesFinalized(const std::vector<ImageResource>& images)
    {
        for (u32 imageIndex = 0; imageIndex < images.size(); imageIndex++)
        {
            auto& image = images.at(imageIndex);
            m_ImageIndexToDescription[imageIndex] = std::format("\timage.{}[\"{}\n\t{} x {} x {}\n\t{}\"]\n",
                imageIndex, image.Name,
                image.Description.Dimensions().x, image.Description.Dimensions().y, image.Description.Dimensions().z,
                ImageTraits::imageUsageToString(image.Description.Usage));
        }
    }

    void RGMermaidExporter::OnBarrierAdded(const BarrierInfo& barrierInfo,
        const Pass& firstPass, const Pass& secondPass)
    {
        const u32 resourceIndex = GetResourceIndex(barrierInfo.Resource);
        std::string tag;
        if (barrierInfo.Resource.IsBuffer())
        {
            tag = "buffer";
            if (!m_DumpedBufferDescriptions.contains(resourceIndex))
            {
                m_Stream << m_BufferIndexToDescription[resourceIndex];
                m_DumpedBufferDescriptions.insert(resourceIndex);
            }
        }
        else
        {
            tag = "image";
            if (!m_DumpedImageDescriptions.contains(resourceIndex))
            {
                m_Stream << m_ImageIndexToDescription[resourceIndex];
                m_DumpedImageDescriptions.insert(resourceIndex);
            }
        }
        
        if (&firstPass != &secondPass)
            m_Stream << std::format("\tpass.\"{}\" --> {}.{}\n", firstPass.Name().Hash(), tag, resourceIndex);
        
        m_Stream << std::format("\t{}.{} --> pass.\"{}\"\n", tag, resourceIndex, secondPass.Name().Hash());
    }
    
    void RGMermaidExporter::OnReset()
    {
        m_Stream.str({});
        m_Stream << std::format("graph LR\n");
        m_BufferIndexToDescription = {};
        m_ImageIndexToDescription = {};
        m_DumpedBufferDescriptions = {};
        m_DumpedImageDescriptions = {};
    }

    void RGMermaidExporter::ExportToHtml(const std::filesystem::path& outputPath) const
    {
        std::filesystem::create_directories(outputPath.parent_path());
        std::ofstream out(outputPath);

        static constexpr std::string_view templateString = R"(
            <!DOCTYPE html>
            <html lang="en">
            <head>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <title>Mermaid Flowchart</title>
                <style type="text/css">
                    #mySvgId {{
                        height: 90%;
                        width: 90%;
                    }}
                </style>
            </head>

            <body>
                <div id="graphDiv"></div>
                <script src="https://bumbu.me/svg-pan-zoom/dist/svg-pan-zoom.js"></script>
                <script type="module">
                    import mermaid from 'https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.esm.min.mjs';

                    mermaid.initialize({{
                        startOnLoad: true,
                        maxTextSize: Number.MAX_SAFE_INTEGER,
                        maxEdges: Number.MAX_SAFE_INTEGER
                    }});

                    const drawDiagram = async function () {{
                        const element = document.querySelector('#graphDiv');
                        const graphDefinition = `
                            {}
                        `;
                        const {{ svg }} = await mermaid.render('mySvgId', graphDefinition);
                        element.innerHTML = svg.replace(/[ ]*max-width:[ 0-9\.]*px;/i, '');
                        var panZoomTiger = svgPanZoom('#mySvgId', {{
                            zoomEnabled: true,
                            controlIconsEnabled: true,
                            fit: true,
                            center: true
                        }})
                    }};
                    await drawDiagram();
                    document.getElementById('mySvgId').setAttribute("height", "100vh");
                    document.getElementById('mySvgId').setAttribute("width", "100vw");
                </script>
            </body>
            </html>
        )";
        
        std::print(out, templateString, m_Stream.str());
    }
}
