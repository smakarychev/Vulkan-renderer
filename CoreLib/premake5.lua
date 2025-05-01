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
    }

    links {
        "efsw",
    }

    filter "system:Linux"
        removefiles {
            "src/Platform/Windows/**"
        }

    filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"