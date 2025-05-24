project "RendererTests"
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
        IncludeDir["Renderer"],
        IncludeDir["CoreLib"],
        IncludeDir["glm"],
        IncludeDir["catch2"],
        IncludeDir["imgui"],
    }

    links {
        "VulkanRendererLib",
        "catch2",    
    }

    defines {
        "VK_NO_PROTOTYPES",
    }