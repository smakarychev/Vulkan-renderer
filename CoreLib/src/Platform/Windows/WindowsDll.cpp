#include "core.h"
#include "Platform/Dll/Dll.h"

#include "WindowsInclude.h"

namespace platform
{
    DllHandle dllOpen(const std::filesystem::path& path)
    {
        const HMODULE module = LoadLibrary(path.c_str());

        return module != nullptr ? (DllHandle)module : INVALID_DLL_HANDLE;
    }

    void dllFree(DllHandle dll)
    {
        FreeLibrary((HMODULE)dll);
    }

    void* dllLoadFunction(DllHandle dll, std::string_view name)
    {
        ASSERT(dll != INVALID_DLL_HANDLE, "Invalid dll handle")

        return GetProcAddress((HMODULE)dll, name.data());
    }
}
