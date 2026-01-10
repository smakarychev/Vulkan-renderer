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
        "src",
        "%{wks.location}/CoreLib/src",
        "%{wks.location}/tools/AssetLib/src",
        "%{wks.location}/tools/AssetConverter/src",
        
        IncludeDir["nlohmann-json"],
        IncludeDir["glaze"],
        IncludeDir["glm"],
    }

    links {
        "CoreLib",
        "AssetLib",
        "AssetConverterLib",
    }

	filter "configurations:Release*"
        postbuildcommands { 
            "{COPYDIR} %{cfg.buildtarget.directory}*.exe %{tools_bindir}%{prj.name}/ > nul",
            "{COPYDIR} %{prj.location.directory}templates %{tools_bindir}%{prj.name}/templates/ > nul" ,
            "{COPYDIR} %{prj.location.directory}resources/* %{tools_bindir}%{prj.name}/ > nul" 
        }
		
