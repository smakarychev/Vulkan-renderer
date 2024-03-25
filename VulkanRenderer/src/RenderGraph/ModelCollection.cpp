#include "ModelCollection.h"

#include "Model.h"
#include "RenderObject.h"
#include "Rendering/Image.h"
#include "Rendering/Shader.h"

void ModelCollection::CreateDefaultTextures()
{
    // add default white texture, so that every material has a texture (to avoid branching in shaders)
    m_WhiteTexture = AddRenderHandle(ImageUtils::DefaultTextures::GetCopy(ImageUtils::DefaultTexture::White),
        m_Textures);
}

void ModelCollection::RegisterModel(Model* model, const std::string& name)
{
    if (m_Models.contains(name))
    {
        if (m_Models.at(name).Model != model)
            LOG("WARNING: overwriting the model {} ({}) with {}",
                name, (void*)m_Models.at(name).Model, (void*)model);
        else
            return;
    }

    m_Models.emplace(name, ModelInfo{
        .Model = model,
        .RenderObjects = CreateRenderObjects(model)});
}

void ModelCollection::AddModelInstance(const std::string& modelName, const ModelInstanceInfo& modelInstanceInfo)
{
    if (!m_Models.contains(modelName))
    {
        LOG("Uknown model name: {}", modelName);
        return;
    }

    for (auto& renderObject : GetRenderObjects(modelName))
        m_RenderObjects.push_back({
            .Mesh = renderObject.Mesh,
            .MaterialGPU = renderObject.MaterialGPU,
            .Material = renderObject.Material,
            .Transform = modelInstanceInfo.Transform});
}

void ModelCollection::ApplyMaterialTextures(ShaderDescriptorSet& bindlessDescriptorSet) const
{
    for (u32 textureIndex = 0; textureIndex < m_Textures.size(); textureIndex++)
    {
        const Texture& texture = m_Textures[textureIndex];
        bindlessDescriptorSet.SetTexture("u_textures", texture, textureIndex);
    }
}

void ModelCollection::ApplyMaterialTextures(ShaderDescriptors& bindlessDescriptors) const
{
    ShaderDescriptors::BindingInfo bindingInfo = bindlessDescriptors.GetBindingInfo("u_textures");
    
    for (u32 textureIndex = 0; textureIndex < m_Textures.size(); textureIndex++)
    {
        const Texture& texture = m_Textures[textureIndex];
        bindlessDescriptors.UpdateBinding(bindingInfo,
            texture.CreateBindingInfo(ImageFilter::Linear, ImageLayout::ReadOnly), textureIndex);
    }
}

const std::vector<RenderObject>& ModelCollection::GetRenderObjects(const std::string& modelName) const
{
    ASSERT(m_Models.contains(modelName), "Unknown model name: {}", modelName)

    return m_Models.at(modelName).RenderObjects;
}

std::vector<RenderObject> ModelCollection::CreateRenderObjects(const Model* model)
{
    std::vector<RenderObject> renderObjects;
    renderObjects.reserve(model->m_Meshes.size());
    for (auto& mesh : model->m_Meshes)
    {
        MaterialGPU material = {
            .AlbedoTextureHandle = m_WhiteTexture,
            .NormalTextureHandle = m_WhiteTexture,
            .MetallicRoughnessTextureHandle = m_WhiteTexture,
            .AmbientOcclusionTextureHandle = m_WhiteTexture};
        material.Albedo = mesh.Material.PropertiesPBR.Albedo;
        material.Metallic = mesh.Material.PropertiesPBR.Metallic;
        material.Roughness = mesh.Material.PropertiesPBR.Roughness;
        auto addTexture = [](MaterialGPU& materialGPU, const std::vector<std::string>& textures, auto&& fn)
        {
            if (!textures.empty())
            {
                Image texture = Image::Builder()
                    .FromAssetFile(textures.front())
                    .SetUsage(ImageUsage::Sampled)
                    .CreateMipmaps(true, ImageFilter::Linear)
                    .Build();

                fn(materialGPU, texture);
            }
        };

        addTexture(material, mesh.Material.AlbedoTextures,
            [this](MaterialGPU& materialGPU, const Image& texture)
            {
                materialGPU.AlbedoTextureHandle = AddTexture(texture);
            });

        addTexture(material, mesh.Material.NormalTextures,
            [this](MaterialGPU& materialGPU, const Image& texture)
            {
                materialGPU.NormalTextureHandle = AddTexture(texture);
            });

        addTexture(material, mesh.Material.MetallicRoughnessTextures,
            [this](MaterialGPU& materialGPU, const Image& texture)
            {
                materialGPU.MetallicRoughnessTextureHandle = AddTexture(texture);
            });

        addTexture(material, mesh.Material.AmbientOcclusionTextures,
            [this](MaterialGPU& materialGPU, const Image& texture)
            {
                materialGPU.AmbientOcclusionTextureHandle = AddTexture(texture);
            });
            
        RenderObject renderObject = {};
        renderObject.Mesh = AddMesh(mesh.Mesh);
        renderObject.MaterialGPU = AddMaterialGPU(material);
        renderObject.Material = AddMaterial(mesh.Material);
        renderObjects.push_back(renderObject);
    }

    return renderObjects;
}

RenderHandle<MaterialGPU> ModelCollection::AddMaterialGPU(const MaterialGPU& material)
{
    return AddRenderHandle(material, m_MaterialsGPU);
}

RenderHandle<Material> ModelCollection::AddMaterial(const Material& material)
{
    return AddRenderHandle(material, m_Materials);
}

RenderHandle<Mesh> ModelCollection::AddMesh(const Mesh& mesh)
{
    return AddRenderHandle(mesh, m_Meshes);
}

RenderHandle<Texture> ModelCollection::AddTexture(const Texture& texture)
{
    return AddRenderHandle(texture, m_Textures);
}
