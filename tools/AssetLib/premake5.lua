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
        "C:/VulkanSDK/1.3.261.1/Include",
        "%{wks.location}/VulkanRenderer/src",
        IncludeDir["lz4"],
        IncludeDir["nlohmann-json"],
        IncludeDir["glm"],
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