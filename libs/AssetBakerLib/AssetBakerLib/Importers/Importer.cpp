#include "Importer.h"

namespace lux::bakers
{
ImportResult<void> Importer::Import(const std::filesystem::path& path)
{
    return Import(path, ImportFlags::BakeIfNotBaked | ImportFlags::Header | ImportFlags::Binaries);
}
}
