#include "ModelCollection.h"

#include "Model.h"
#include "Rendering/Image/Image.h"
#include "Rendering/Shader.h"
#include "Vulkan/Driver.h"

void ModelCollection::CreateDefaultTextures()
{
    // add default white texture, so that every material has a texture (to avoid branching in shaders)
    m_WhiteTexture = AddRenderHandle(ImageUtils::DefaultTextures::GetCopy(
        ImageUtils::DefaultTexture::White, Driver::DeletionQueue()),
        m_Textures);

    // black is default emissive texture
    m_BlackTexture = AddRenderHandle(ImageUtils::DefaultTextures::GetCopy(
        ImageUtils::DefaultTexture::Black, Driver::DeletionQueue()),
        m_Textures);

    m_NormalMapTexture = AddRenderHandle(ImageUtils::DefaultTextures::GetCopy(
        ImageUtils::DefaultTexture::NormalMap, Driver::DeletionQueue()),
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
        LOG("Unknown model name: {}", modelName);
        return;
    }

    for (auto& renderObject : GetRenderObjects(modelName))
    {
        m_RenderObjects.push_back({
            .Mesh = renderObject.Mesh,
            .MaterialGPU = renderObject.MaterialGPU,
            .Material = renderObject.Material,
            .Transform = modelInstanceInfo.Transform});

        /* here we merge a bounds of the individual meshes to produce a
         * bounding box for the entire geometry.
         * !!NOTE!! that because the default bounds have min and max
         * set to 0, the resulting bounding box will always contain world origin
         */
        m_BoundingBox.Merge(AABB::Transform(m_Meshes[renderObject.Mesh].GetBoundingBox(),
            modelInstanceInfo.Transform.Position,
            modelInstanceInfo.Transform.Orientation,
            modelInstanceInfo.Transform.Scale));
    }
}

void ModelCollection::ApplyMaterialTextures(ShaderDescriptorSet& bindlessDescriptorSet) const
{
    for (u32 textureIndex = 0; textureIndex < m_Textures.size(); textureIndex++)
    {
        const Texture& texture = m_Textures[textureIndex];
        bindlessDescriptorSet.SetTexture(UNIFORM_TEXTURES, texture, textureIndex);
    }
}

void ModelCollection::ApplyMaterialTextures(ShaderDescriptors& bindlessDescriptors) const
{
    ShaderDescriptors::BindingInfo bindingInfo = bindlessDescriptors.GetBindingInfo(UNIFORM_TEXTURES);
    
    for (u32 textureIndex = 0; textureIndex < m_Textures.size(); textureIndex++)
    {
        const Texture& texture = m_Textures[textureIndex];
        bindlessDescriptors.UpdateGlobalBinding(bindingInfo,
            texture.BindingInfo(ImageFilter::Linear, ImageLayout::Readonly), textureIndex);
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
            .NormalTextureHandle = m_NormalMapTexture,
            .MetallicRoughnessTextureHandle = m_WhiteTexture,
            .AmbientOcclusionTextureHandle = m_WhiteTexture,
            .EmissiveTextureHandle = m_BlackTexture};
        material.Albedo = mesh.Material.PropertiesPBR.Albedo;
        material.Metallic = mesh.Material.PropertiesPBR.Metallic;
        material.Roughness = mesh.Material.PropertiesPBR.Roughness;
        auto addTexture = [this](MaterialGPU& materialGPU, const std::vector<std::string>& textures, auto&& fn)
        {
            if (!textures.empty())
            {
                if (m_TexturesMap.contains(textures.front()))
                {
                    *fn(materialGPU) = m_TexturesMap.at(textures.front());
                }
                else
                {
                    Image texture = Image::Builder({.Usage = ImageUsage::Sampled})
                        .FromAssetFile(textures.front())
                        .Build();
                    
                    *fn(materialGPU) = AddTexture(texture);
                    m_TexturesMap.emplace(textures.front(), *fn(materialGPU));
                }
            }
        };

        addTexture(material, mesh.Material.AlbedoTextures,
            [this](MaterialGPU& materialGPU)
            {
                return &materialGPU.AlbedoTextureHandle;
            });

        addTexture(material, mesh.Material.NormalTextures,
            [this](MaterialGPU& materialGPU)
            {
                return &materialGPU.NormalTextureHandle;
            });

        addTexture(material, mesh.Material.MetallicRoughnessTextures,
            [this](MaterialGPU& materialGPU)
            {
                return &materialGPU.MetallicRoughnessTextureHandle;
            });

        addTexture(material, mesh.Material.AmbientOcclusionTextures,
            [this](MaterialGPU& materialGPU)
            {
                return &materialGPU.AmbientOcclusionTextureHandle;
            });
        
        addTexture(material, mesh.Material.EmissiveTextures,
            [this](MaterialGPU& materialGPU)
            {
                return &materialGPU.EmissiveTextureHandle;
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
