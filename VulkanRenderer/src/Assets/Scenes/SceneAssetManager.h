#pragma once
#include "SceneAsset.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetSystem.h"
#include "Assets/Common/AssetSlotMap.h"
#include "Scene/BindlessTextureDescriptorsRingBuffer.h"

#include <AssetBakerLib/Bakers/BakerContext.h>
#include <CoreLib/Signals/Signal.h>

struct FrameContext;

namespace lux
{
class MaterialAssetManager;
class ImageAssetManager;

namespace bakers
{
struct SceneBakeSettings;
}

template <>
struct ResourceAssetLoadParameters<SceneAsset>
{
    std::filesystem::path Path{};
};

using SceneLoadParameters = ResourceAssetLoadParameters<SceneAsset>;

class SceneAssetManager final : public ResourceAssetManager<SceneAsset, ResourceAssetTraits>
{
public:
    struct SceneDeletedInfo
    {
        SceneHandle Scene{};
    };

    struct SceneReplacedInfo
    {
        SceneHandle Scene{};
    };
    
    struct MaterialUpdatedInfo
    {
        SceneHandle Scene{};
    };

    using SceneDeletedSignal = Signal<SceneDeletedInfo>;
    using SceneReplacedSignal = Signal<SceneReplacedInfo>;
    using MaterialUpdatedSignal = Signal<MaterialUpdatedInfo>;

public:
    LUX_ASSET_MANAGER(SceneAssetManager, "87aebbdf-a3c4-4d65-9ffd-314dcd26ba01"_guid)

    void OnAssetSystemInit() override;
    bool AddManaged(const std::filesystem::path& path, AssetIdResolver& resolver) override;
    bool Bakes(const std::filesystem::path& path) override;
    void OnFileModified(const std::filesystem::path& path) override;

    void Init(const bakers::SceneBakeSettings& bakeSettings);
    void SetTextureRingBuffer(BindlessTextureDescriptorsRingBuffer& ringBuffer);
    SceneDeletedSignal& GetSceneDeletedSignal() { return m_SceneDeletedSignal; }
    SceneReplacedSignal& GetSceneReplacedSignal() { return m_SceneReplacedSignal; }
    MaterialUpdatedSignal& GetMaterialUpdatedSignal() { return m_MaterialUpdatedSignal; }

    SceneHandle AddExternalScene(SceneAsset&& scene);
    
    void OnFrameBegin(FrameContext& ctx);

protected:
    SceneHandle LoadAsset(const SceneLoadParameters& parameters) override;
    void UnloadAsset(SceneHandle handle) override;
    GetType GetAsset(SceneHandle handle) const override;

private:
    void OnRawFileModified(const std::filesystem::path& path);
    void OnBakedFileModified(const std::filesystem::path& path);
    void OnMaterialUpdated(MaterialHandle material);
    void OnTextureUpdated(ImageHandle texture);
    void RegisterMaterials(SceneHandle sceneHandle);
    void UnregisterMaterials(SceneHandle sceneHandle);
    std::optional<SceneAsset> DoLoad(const SceneLoadParameters& parameters);
    SceneGeometryInfo LoadGeometryInfo(assetlib::SceneAsset& scene);
    SceneLightInfo LoadLightsInfo(assetlib::SceneAsset& scene);
    SceneHierarchyInfo LoadHierarchyInfo(assetlib::SceneAsset& scene);
    void LoadMaterials(SceneGeometryInfo& geometry, assetlib::SceneAsset& scene);
    MaterialGPU LoadMaterial(const SceneGeometryInfo::MaterialInfo& materialInfo, const MaterialAsset& materialAsset);
    TextureHandle LoadTexture(u32 uvIndex, ImageHandle image, TextureHandle fallback);

private:
    AssetSlotMap<SceneAsset> m_Scenes;
    
    BindlessTextureDescriptorsRingBuffer* m_TexturesRingBuffer{nullptr};
    std::unordered_map<ImageHandle, TextureHandle> m_LoadedTextures;
    
    ImageAssetManager* m_TextureAssetManager{nullptr};
    MaterialAssetManager* m_MaterialAssetManager{nullptr};

    enum class UnloadState : u8
    {
        Queued = 0, Unload = 1,
        MaxValue = 2
    };

    std::array<std::vector<SceneHandle>, (u32)UnloadState::MaxValue> m_ToUnload;
    SceneDeletedSignal m_SceneDeletedSignal;
    
    /* for hot-reloading */
    bakers::Context m_Context{};
    const bakers::SceneBakeSettings* m_BakeSettings{nullptr};
    AssetUpdatedHandler m_MaterialUpdatedHandler;
    AssetUpdatedHandler m_TextureUpdatedHandler;
    std::unordered_map<MaterialHandle, std::vector<SceneHandle>> m_MaterialToScenes;
    SceneReplacedSignal m_SceneReplacedSignal;
    MaterialUpdatedSignal m_MaterialUpdatedSignal;
};
}
