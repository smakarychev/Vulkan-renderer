project "CoreLibTests"
    kind "ConsoleApp"
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
        IncludeDir["CoreLib"],
        IncludeDir["catch2"],
    }

    links {
        "CoreLib",
        "catch2",    
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