workspace "vulkan-tutorial"
    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
    configurations { "Debug", "Release"}
    architecture "x86_64"
    flags
	{
		"MultiProcessorCompile"
	}
    startproject "VulkanApp"

    linkoptions { "/NODEFAULTLIB:LIBCMTD.LIB" }

    
group "Dependencies"
include "vendor/glfw"
include "vendor/glm"

group ""
project "VulkanApp"
	kind "ConsoleApp"
	language "C++"
    cppdialect "C++20"
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
    {
        "src/**.h",
        "src/**.cpp",
    }

    includedirs
    {
        "C:/VulkanSDK/1.3.236.0/Include",
        "src",
        "vendor/glfw/include",
        "vendor/glm",
    }

    libdirs
	{
		"C:/VulkanSDK/1.3.236.0/Lib"
	}

    links
    {
        "vulkan-1.lib",
        "glfw",
    }

    defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE",
	}


	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"
		defines { "VULKAN_VAL_LAYERS" }
        links {
            "shadercd.lib",
            "shaderc_combinedd.lib", 
            "shaderc_sharedd.lib", 
            "shaderc_utild.lib"
        }

	filter "configurations:Release"
		runtime "Release"
		optimize "on"
        links {
            "shaderc.lib",
            "shaderc_combined.lib", 
            "shaderc_shared.lib", 
            "shaderc_util.lib"
        }