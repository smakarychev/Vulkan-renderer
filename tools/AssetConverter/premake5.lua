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
        "C:/VulkanSDK/1.3.261.1/Include",
        "%{wks.location}/tools/AssetLib/src",
	
        IncludeDir["stb"],		    
        IncludeDir["assimp"],
        IncludeDir["glm"],
        IncludeDir["spirv_reflect"],	
        IncludeDir["meshoptimizer"],    
    }

    libdirs
	{
		"C:/VulkanSDK/1.3.261.1/Lib",
        "%{wks.location}/vendor/assimp/lib",
	}

    links
    {
        "AssetLib",
        "assimp-vc143-mt.lib",
        "meshoptimizer"
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