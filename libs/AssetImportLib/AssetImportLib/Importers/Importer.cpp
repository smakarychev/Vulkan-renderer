#include "Importer.h"

namespace lux::import
{
ImportResult<void> Importer::Import(const std::filesystem::path& path)
{
    return Import(path, ImportFlags::BakeIfNotBaked | ImportFlags::Header | ImportFlags::Binaries);
}
}
