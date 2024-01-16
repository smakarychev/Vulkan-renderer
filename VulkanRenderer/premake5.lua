project "VulkanRenderer"
	kind "ConsoleApp"
	language "C++"
    cppdialect "C++20"
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
    {
        "src/**.h",
        "src/**.cpp",
    }

    includedirs
    {
        "src",
        "%{wks.location}/tools/AssetLib/src",
        IncludeDir["GLFW"],			
        IncludeDir["glm"],			
        IncludeDir["vma"],	
        IncludeDir["spirv_reflect"],	
        IncludeDir["tracy"],    
        IncludeDir["volk"],
        "$(VULKAN_SDK)/Include",
    }

    libdirs
	{
	}

    links
    {
        "glfw",
        "AssetLib",
    }

    defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE",
        "TRACY_ENABLE",
        "VK_NO_PROTOTYPES",
	}


	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"
		defines { "VULKAN_VAL_LAYERS" }
        links {
        }

	filter "configurations:Release"
		runtime "Release"
		optimize "on"
        links {
        }