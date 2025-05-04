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
        IncludeDir["glm"],
    }

    links {
        "CoreLib",
        "catch2",    
    }

    filter "system:Linux"
        removefiles {
            "src/Platform/Windows/**"
        }