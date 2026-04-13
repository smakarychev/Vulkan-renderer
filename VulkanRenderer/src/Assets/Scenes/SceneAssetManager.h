#pragma once
#include "SceneAsset.h"
#include "Assets/AssetManager.h"
#include "Assets/Common/AssetSlotMap.h"
#include "Scene/BindlessTextureDescriptorsRingBuffer.h"

#include <AssetBakerLib/Bakers/BakerContext.h>
#include <CoreLib/Signals/Signal.h>

struct FrameContext;

namespace lux
{
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
        SceneHandle Original{};
        SceneHandle Replaced{};
    };

    using SceneDeletedSignal = Signal<SceneDeletedInfo>;
    using SceneReplacedSignal = Signal<SceneReplacedInfo>;

public:
    LUX_ASSET_MANAGER(SceneAssetManager, "87aebbdf-a3c4-4d65-9ffd-314dcd26ba01"_guid)

    bool AddManaged(const std::filesystem::path& path, AssetIdResolver& resolver) override;
    bool Bakes(const std::filesystem::path& path) override;
    void OnFileModified(const std::filesystem::path& path) override;

    void Init(const bakers::SceneBakeSettings& bakeSettings);
    void SetTextureRingBuffer(BindlessTextureDescriptorsRingBuffer& ringBuffer);
    const SceneDeletedSignal& GetSceneDeletedSignal() const { return m_SceneDeletedSignal; }
    const SceneReplacedSignal& GetSceneReplacedSignal() const { return m_SceneReplacedSignal; }

    void OnFrameBegin(FrameContext& ctx);

protected:
    SceneHandle LoadAsset(const SceneLoadParameters& parameters) override;
    void UnloadAsset(SceneHandle handle) override;
    GetType GetAsset(SceneHandle handle) const override;

private:
    void OnRawFileModified(const std::filesystem::path& path);
    void OnBakedFileModified(const std::filesystem::path& path);
    SceneAsset DoLoad(const SceneLoadParameters& parameters) const;

private:
    BindlessTextureDescriptorsRingBuffer* m_TexturesRingBuffer{nullptr};

    AssetSlotMap<SceneAsset> m_Scenes;

    enum class UnloadState : u8
    {
        Queued = 0, Unload = 1,
        MaxValue = 2
    };

    std::array<std::vector<SceneHandle>, (u32)UnloadState::MaxValue> m_ToUnload;
    SceneDeletedSignal m_SceneDeletedSignal;
    SceneReplacedSignal m_SceneReplacedSignal;
    
    
    /* for hot-reloading */
    bakers::Context m_Context{};
    const bakers::SceneBakeSettings* m_BakeSettings{nullptr};
};
}
