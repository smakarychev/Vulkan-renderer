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
        "%{wks.location}/CoreLib/src",
        IncludeDir["lz4"],
        IncludeDir["nlohmann-json"],
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

    filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"