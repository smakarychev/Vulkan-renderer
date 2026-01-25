project "AssetConverter"
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
        "$(VULKAN_SDK)/Include",
	    "%{wks.location}/CoreLib/src",
        "%{wks.location}/tools/AssetLib/src",
	    
        IncludeDir["stb"],		    
        IncludeDir["glm"],
        IncludeDir["meshoptimizer"],   
        IncludeDir["tinygltf"],    
        IncludeDir["mikktspace"],        
        IncludeDir["nlohmann-json"],
        IncludeDir["glaze"],
    }

    libdirs {
		"$(VULKAN_SDK)/Lib",
	}

    links {
        "CoreLib",
        "AssetLib",
        "meshoptimizer",
        "mikktspace",
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
    
project "AssetConverterLib"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files {
        "src/**.h",
        "src/**.cpp",
    }
    removefiles {
        "src/main.cpp",
    }

    includedirs {
        "src",
        "$(VULKAN_SDK)/Include",
        "%{wks.location}/CoreLib/src",
        "%{wks.location}/tools/AssetLib/src",

        IncludeDir["stb"],		    
        IncludeDir["glm"],
        IncludeDir["meshoptimizer"],    
        IncludeDir["tinygltf"],    
        IncludeDir["mikktspace"],    
        IncludeDir["nlohmann-json"],
        IncludeDir["glaze"],
    }

    libdirs {
		"$(VULKAN_SDK)/Lib",
	}

    links {
        "CoreLib",
        "AssetLib",
        "meshoptimizer",
        "mikktspace",
    }

    filter "configurations:Debug*"
        links {
            "slangd.lib",
        }

    filter "configurations:Release*"
        links {
            "slang.lib",
        }