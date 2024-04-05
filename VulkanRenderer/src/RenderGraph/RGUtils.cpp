#include "RGUtils.h"

#include "RenderGraph.h"
#include "General/DrawResources.h"

namespace RenderGraph::RgUtils
{
    Resource ensureResource(Resource resource, Graph& graph, const std::string& name,
        const GraphTextureDescription& fallback)
    {
        return resource.IsValid() ?
            resource :
            graph.CreateResource(name, fallback);
    }

    Resource ensureResource(Resource resource, Graph& graph, const std::string& name,
        ImageUtils::DefaultTexture fallback)
    {
        return resource.IsValid() ?
            resource :
            graph.AddExternal(name, fallback);
    }

    void updateIBLBindings(const ShaderDescriptors& descriptors, const Resources& resources, const IBLData& iblData)
    {
        const Texture& irradiance = resources.GetTexture(iblData.Irradiance);
        const Texture& prefilter = resources.GetTexture(iblData.PrefilterEnvironment);
        const Texture& brdf = resources.GetTexture(iblData.BRDF);

        descriptors.UpdateBinding("u_irradiance_map", irradiance.BindingInfo(
            ImageFilter::Linear, ImageLayout::Readonly));
        descriptors.UpdateBinding("u_prefilter_map", prefilter.BindingInfo(
            ImageFilter::Linear, ImageLayout::Readonly));
        descriptors.UpdateBinding("u_brdf", brdf.BindingInfo(
            ImageFilter::Linear, ImageLayout::Readonly));
    }

    void updateSSAOBindings(const ShaderDescriptors& descriptors, const Resources& resources, const SSAOData& ssaoData)
    {
        const Texture& ssao = resources.GetTexture(ssaoData.SSAOTexture);

        descriptors.UpdateBinding("u_ssao_texture", ssao.BindingInfo(
            ImageFilter::Linear, ImageLayout::Readonly));
    }
}
