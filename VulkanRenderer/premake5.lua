project "VulkanRenderer"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    
    files {
        "src/**.h",
        "src/**.cpp",
        "src/RenderGraph/Passes/Generated/**",
        --"src/RenderGraph/Passes/Generated/ShaderBindGroups.generated.h"
    }

    includedirs {
        "src",
        "%{wks.location}/CoreLib/src",
        "%{wks.location}/tools/AssetLib/src",
        "%{wks.location}/tools/AssetConverter/src",
        IncludeDir["GLFW"],			
        IncludeDir["glm"],			
        IncludeDir["vma"],	
        IncludeDir["spirv_reflect"],	
        IncludeDir["tracy"],    
        IncludeDir["volk"],
        IncludeDir["tinygltf"],  
        IncludeDir["imgui"],
        IncludeDir["nlohmann-json"],
        IncludeDir["efsw"],
        "$(VULKAN_SDK)/Include",
    }

    libdirs {
    }

    links {
        "glfw",
        "imgui",
        "efsw",
        "CoreLib",
        "AssetLib",
        "AssetConverterLib",
    }

    defines {
        "_CRT_SECURE_NO_WARNINGS",
        "GLFW_INCLUDE_NONE",
        "TRACY_ENABLE",
        "VK_NO_PROTOTYPES",
    }

    prebuildcommands {
        "\
        if not exist %{tools_bindir}/ShaderBindGroupGen/ShaderBindGroupGen.exe ( \
        msbuild %{wks.location}tools/ShaderBindGroupGen /p:Configuration=Release /p:Platform=x64) \
        %{tools_bindir}ShaderBindGroupGen/ShaderBindGroupGen.exe %{wks.location}/assets/shaders %{prj.location}src/RenderGraph/Passes/Generated/ \
        :: this is unholy \
        cd %{wks.location} \
        %{wks.location}build.bat"
    }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"
        defines { 
            "VULKAN_VAL_LAYERS",
        }
        links {
        }

    filter "configurations:Release"
        runtime "Release"
        optimize "on"
        links {
        }