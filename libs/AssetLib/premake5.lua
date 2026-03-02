project "AssetLib"
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    
    files {
        "AssetLib/**.h",
        "AssetLib/**.cpp",
    }

    includedirs {
        "./",
        IncludeDir["CoreLib"],
    }
    
    externalincludedirs {
        IncludeDir["lz4"],
        IncludeDir["glaze"],
        IncludeDir["glm"],
        IncludeDir["volk"],
        "$(VULKAN_SDK)/Include",
    }

    defines {
        "VK_NO_PROTOTYPES",
    }

    links {
        "lz4",
        "CoreLib",
    }