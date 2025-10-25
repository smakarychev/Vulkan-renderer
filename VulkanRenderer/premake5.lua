local project_files = {
    "src/**.h",
    "src/**.cpp",
    "src/RenderGraph/Passes/Generated/**",
}

local project_includes = {
    "src",
    "%{wks.location}/CoreLib/src",
    "%{wks.location}/tools/AssetLib/src",
    "%{wks.location}/tools/AssetConverter/src",
    IncludeDir["GLFW"],			
    IncludeDir["glm"],			
    IncludeDir["vma"],	
    IncludeDir["spirv_reflect"],	
    IncludeDir["tracy"],    
    IncludeDir["volk"],
    IncludeDir["tinygltf"],  
    IncludeDir["imgui"],
    IncludeDir["nlohmann-json"],
    "$(VULKAN_SDK)/Include",
}

local project_libdirs = {
}

local project_links = {
    "glfw",
    "imgui",
    "tracy",
    "CoreLib",
    "AssetLib",
    "AssetConverterLib",
}

local project_defines = {
    "_CRT_SECURE_NO_WARNINGS",
    "GLFW_INCLUDE_NONE",
    "TRACY_ENABLE",
    "VK_NO_PROTOTYPES",
}

local project_debug_defines = {
    "VULKAN_VAL_LAYERS",
}

local project_debug_descriptor_buffer_defines = {
    "VULKAN_VAL_LAYERS",
    "DESCRIPTOR_BUFFER",
}

local project_release_descriptor_buffer_defines = {
    "DESCRIPTOR_BUFFER",
}

project "VulkanRenderer"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    
    files (project_files)
    includedirs (project_includes)
    libdirs (project_libdirs)
    links (project_links)
    defines (project_defines)

    pchheader "rendererpch.h"
    pchsource "src/rendererpch.cpp"

    prebuildcommands {
        "\
        if not exist %{tools_bindir}/AssetConverter.exe (\
            msbuild %{wks.location}tools/AssetConverter /p:Configuration=Release /p:Platform=x64) \
        if not exist %{tools_bindir}/ShaderBindGroupGen/ShaderBindGroupGen.exe ( \
            msbuild %{wks.location}tools/ShaderBindGroupGen /p:Configuration=Release /p:Platform=x64) \
        cmd.exe /c %{tools_bindir}AssetConverter.exe %{wks.location}/assets/shaders > nul \
        cmd.exe /c %{tools_bindir}ShaderBindGroupGen/ShaderBindGroupGen.exe %{wks.location}/assets/shaders %{prj.location}src/RenderGraph/Passes/Generated/ \
        :: this is unholy \
        cd %{wks.location} \
        %{wks.location}build.bat"
    }

    filter "configurations:Debug"
        defines (project_debug_defines)
    
    filter "configurations:Debug_DescriptorBuffer"
        defines (project_debug_descriptor_buffer_defines)

    filter "configurations:Release_DescriptorBuffer"
        defines (project_release_descriptor_buffer_defines)
        
project "VulkanRendererLib"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    
    files (project_files)
    removefiles { "src/main.cpp" }
    
    includedirs (project_includes)
    libdirs (project_libdirs)
    links (project_links)
    defines (project_defines)

    pchheader "rendererpch.h"
    pchsource "src/rendererpch.cpp"

    filter "configurations:Debug"
        defines (project_debug_defines)
    
    filter "configurations:Debug_DescriptorBuffer"
        defines (project_debug_descriptor_buffer_defines)

    filter "configurations:Release_DescriptorBuffer"
        defines (project_release_descriptor_buffer_defines)
