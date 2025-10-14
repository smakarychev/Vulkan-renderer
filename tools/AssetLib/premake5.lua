project "AssetLib"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    
    files {
        "src/**.h",
        "src/**.cpp",
    }

    includedirs {
        "src",
        "%{wks.location}/CoreLib/src",
        IncludeDir["lz4"],
        IncludeDir["nlohmann-json"],
        IncludeDir["glaze"],
        IncludeDir["glm"],
        IncludeDir["volk"],
        IncludeDir["tinygltf"],  
        "$(VULKAN_SDK)/Include",
    }

    defines {
        "VK_NO_PROTOTYPES",
    }

    links {
        "lz4",
        "CoreLib",
    }