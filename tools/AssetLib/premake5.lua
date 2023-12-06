project "AssetLib"
    kind "StaticLib"
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
        "%{wks.location}/VulkanRenderer/src",
        IncludeDir["lz4"],
        IncludeDir["nlohmann-json"],
        IncludeDir["glm"],
        IncludeDir["volk"],
        "C:/VulkanSDK/1.3.268.0/Include",
    }

    defines 
    {
        "VK_NO_PROTOTYPES",
    }

    links
    {
        "lz4",
    }

    filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"