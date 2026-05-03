project "AssetImporter"
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
        IncludeDir["CoreLib"], 
        IncludeDir["AssetLib"], 
        IncludeDir["AssetImportLib"], 
    }
    externalincludedirs {
        IncludeDir["glaze"],
        IncludeDir["glm"],
    }
    libdirs {
		"$(VULKAN_SDK)/Lib",
	}

    links {
        "AssetImportLib"
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
