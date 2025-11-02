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
        IncludeDir["spirv_reflect"],	
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
            "shadercd.lib",
            "shaderc_combinedd.lib", 
            "shaderc_sharedd.lib", 
            "shaderc_utild.lib",
            "slangd.lib",
        }

    filter "configurations:Release*"
        links {
            "shaderc.lib",
            "shaderc_combined.lib", 
            "shaderc_shared.lib", 
            "shaderc_util.lib",
            "slang.lib",
        }
        postbuildcommands { 
            "{COPYDIR} %{cfg.buildtarget.directory}*.exe %{wks.location}/tools/bin/AssetConverter/ > nul" 
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
        IncludeDir["spirv_reflect"],	
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
            "shadercd.lib",
            "shaderc_combinedd.lib", 
            "shaderc_sharedd.lib", 
            "shaderc_utild.lib",
            "slangd.lib",
        }

    filter "configurations:Release*"
        links {
            "shaderc.lib",
            "shaderc_combined.lib", 
            "shaderc_shared.lib", 
            "shaderc_util.lib",
            "slang.lib",
        }