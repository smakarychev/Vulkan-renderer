#include "AssetId.h"

#include <random>

namespace
{
std::uniform_int_distribution g_Distribution{std::numeric_limits<u64>::min(), std::numeric_limits<u64>::max()};
std::mt19937 g_Mt{std::random_device{}()};
}

namespace assetlib
{
AssetId::AssetId()
    : m_Id(g_Distribution(g_Mt))
{
}

void AssetId::Generate()
{
    m_Id = g_Distribution(g_Mt);
}
}
