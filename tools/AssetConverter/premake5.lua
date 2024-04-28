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
        "$(VULKAN_SDK)/Include",
        "%{wks.location}/tools/AssetLib/src",
	
        IncludeDir["stb"],		    
        IncludeDir["assimp"],
        IncludeDir["glm"],
        IncludeDir["spirv_reflect"],	
        IncludeDir["meshoptimizer"],    
    }

    libdirs
	{
		"$(VULKAN_SDK)/Lib",
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
            "shaderc_utild.lib",
        }

    filter "configurations:Release"
        runtime "Release"
        optimize "on"
        links {
            "shaderc.lib",
            "shaderc_combined.lib", 
            "shaderc_shared.lib", 
            "shaderc_util.lib",
        }