project "ShaderBindGroupGen"
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
        IncludeDir["spirv_reflect"],
        IncludeDir["nlohmann-json"],
        "%{wks.location}/CoreLib/src",
    }

    defines {
    }

    links {
        CoreLib
    }

    filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"