project "imgui"
    kind "StaticLib"
    language "C"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    systemversion "latest"
    staticruntime "On"

    files
    {
        "imgui/**"
    }

    includedirs
    {
        IncludeDir["GLFW"],			
        "$(VULKAN_SDK)/Include",	
    }

    
    defines
    {
        "VK_NO_PROTOTYPES",
    }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"