project "CoreLib"
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    
    files {
        "CoreLib/**.h",
        "CoreLib/**.cpp",
    }

    includedirs {
        "./",
    }
    
    externalincludedirs {
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