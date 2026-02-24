project "CoreLib"
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
        IncludeDir["glm"],
        IncludeDir["efsw"],
        IncludeDir["spdlog"], 
    }

    links {
        "efsw",
    }

    filter "system:Linux"
        removefiles {
            "src/Platform/Windows/**"
        }