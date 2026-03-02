project "AssetBakerLib"
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    defines {
        "KHRONOS_STATIC"
    }

	files {
        "src/**.h",
        "src/**.cpp",
    }

    includedirs {
        "src",
        IncludeDir["CoreLib"],
        IncludeDir["AssetLib"]
    }
    externalincludedirs {
        "$(VULKAN_SDK)/Include",
        IncludeDir["stb"],		    
        IncludeDir["glm"],
        IncludeDir["meshoptimizer"],   
        IncludeDir["tinygltf"],    
        IncludeDir["mikktspace"],        
        IncludeDir["nlohmann-json"],
        IncludeDir["glaze"],
        -- todo: i hate all this 
        IncludeDir["ktx"],
        IncludeDir["ktx"] .. "/../../external/fmt/include",    
        IncludeDir["ktx"] .. "/../../external/cxxopts/include",
        IncludeDir["ktx"] .. "/../../external/",
        IncludeDir["ktx"] .. "/../../external/basis_universal",
        IncludeDir["ktx"] .. "/../../external/basis_universal/zstd",
        IncludeDir["ktx"] .. "/../../external/basis_universal/encoder",
        IncludeDir["ktx"] .. "/../../external/basis_universal/transcoder",
        IncludeDir["ktx"] .. "/../../external/basis_universal/OpenCL", 
        IncludeDir["ktx"] .. "/../src",
        IncludeDir["toolsktx"],
        IncludeDir["utilsktx"], 
        IncludeDir["toolsktx"] .. "/imageio",
    }

    libdirs {
		"$(VULKAN_SDK)/Lib",
	}

    links {
        "CoreLib",
        "AssetLib",
        "meshoptimizer",
        "mikktspace",
        "ktxtools",  
    }

    filter "configurations:Debug*"
        links {
            "slangd.lib",
        }

    filter "configurations:Release*"
        links {
            "slang.lib",
        }