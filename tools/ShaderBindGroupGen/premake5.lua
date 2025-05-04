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
        "%{wks.location}/CoreLib/src",
        "%{wks.location}/tools/AssetLib/src",
        
        IncludeDir["spirv_reflect"],
        IncludeDir["nlohmann-json"],
        IncludeDir["inja"],
    }

    links {
        "CoreLib",
        "AssetLib",
    }

	filter "configurations:Release*"
        postbuildcommands { 
            "{COPYDIR} %{cfg.buildtarget.directory}*.exe %{tools_bindir}%{prj.name}/ > nul \
             {COPYDIR} %{prj.location.directory}templates %{tools_bindir}%{prj.name}/templates/ > nul" 
        }
		
