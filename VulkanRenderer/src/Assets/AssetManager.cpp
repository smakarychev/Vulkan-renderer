#include "rendererpch.h"
#include "AssetManager.h"
#include "AssetSystem.h"

namespace lux
{
AssetManager::AssetManager(): m_Ctx(AssetSystem::Get().GetContext())
{}
}
