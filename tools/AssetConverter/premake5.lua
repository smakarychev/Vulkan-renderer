local project_files = {
    "src/**.h",
    "src/**.cpp",
}

local include_dirs = {
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

local project_links = {
    "CoreLib",
    "AssetLib",
    "meshoptimizer",
    "mikktspace",
    "ktxtools",  
}

local project_defines = {
    "KHRONOS_STATIC"
}

project "AssetConverter"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    defines (project_defines) 

	files (project_files)
 	
    includedirs (include_dirs)
    libdirs {
		"$(VULKAN_SDK)/Lib",
	}

    links (project_links)

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
    defines (project_defines) 

	files (project_files)
    removefiles {
        "src/main.cpp",
    }

    includedirs (include_dirs)

    libdirs {
		"$(VULKAN_SDK)/Lib",
	}

    links (project_links)

    filter "configurations:Debug*"
        links {
            "slangd.lib",
        }

    filter "configurations:Release*"
        links {
            "slang.lib",
        }