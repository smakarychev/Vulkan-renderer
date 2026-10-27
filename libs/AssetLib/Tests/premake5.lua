project "AssetLibTests"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    
    files {
        "src/**.h",
        "src/**.cpp",
    }

    includedirs {
        "src",
    }

    externalincludedirs {
        IncludeDir["CoreLib"],
        IncludeDir["AssetLib"],
        IncludeDir["catch2"],
        IncludeDir["glm"],
    }

    links {
        "AssetLib",
        "catch2",    
    }