project "AssetBaker"
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
        "%{wks.location}/AssetBakerLib/src",
    }
    externalincludedirs {
        IncludeDir["glaze"],
        IncludeDir["glm"],
    }
    libdirs {
		"$(VULKAN_SDK)/Lib",
	}

    links {
        "AssetBakerLib"
    }

    filter "configurations:Debug*"
        links {
            "slangd.lib",
        }

    filter "configurations:Release*"
        links {
            "slang.lib",
        }
        postbuildcommands { 
            "{COPYDIR} %{cfg.buildtarget.directory}*.exe %{tools_bindir}%{prj.name}/ > nul",
            "{COPYDIR} %{prj.location.directory}resources/* %{tools_bindir}%{prj.name}/ > nul" 
        }
