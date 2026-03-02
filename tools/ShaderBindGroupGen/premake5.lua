project "ShaderBindGroupGen"
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
        "%{wks.location}/CoreLib/src",
        "%{wks.location}/AssetLib/src",
        "%{wks.location}/tools/AssetBaker/src",
    }
    
    externalincludedirs {
        IncludeDir["nlohmann-json"],
        IncludeDir["glaze"],
        IncludeDir["glm"],
    }

    links {
        "CoreLib",
        "AssetLib",
        "AssetBakerLib",
    }

	filter "configurations:Release*"
        postbuildcommands { 
            "{COPYDIR} %{cfg.buildtarget.directory}*.exe %{tools_bindir}%{prj.name}/ > nul",
            "{COPYDIR} %{prj.location.directory}templates %{tools_bindir}%{prj.name}/templates/ > nul" ,
            "{COPYDIR} %{prj.location.directory}resources/* %{tools_bindir}%{prj.name}/ > nul" 
        }
		
