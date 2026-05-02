#include "rendererpch.h"
#include "AssetManager.h"
#include "AssetSystem.h"

namespace lux
{
AssetManager::AssetManager(AssetSystem& system): m_AssetSystem(&system), m_Ctx(system.GetContext())
{}
}
