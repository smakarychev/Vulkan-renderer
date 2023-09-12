project "AssetConverter"
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
        "src",
        "C:/VulkanSDK/1.3.236.0/Include",
        "%{wks.location}/tools/AssetLib/src",
        IncludeDir["stb"],		    
        IncludeDir["tinyobjloader"],	
        IncludeDir["glm"],
    }

    libdirs
	{
		"C:/VulkanSDK/1.3.236.0/Lib"
	}

    links
    {
        "AssetLib",
    }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"
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