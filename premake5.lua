include "dependencies.lua"

workspace "VulkanRenderer"
    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
    tools_bindir = "%{wks.location}tools/bin/"
    configurations { "Debug", "Release", "Debug_DescriptorBuffer", "Release_DescriptorBuffer" }
    architecture "x86_64"
    editandcontinue "Off"
    flags {
		"MultiProcessorCompile",
	}
    startproject "VulkanRenderer"

    linkoptions { 
        "/IGNORE:4221,4099,4006,4098",
    }

    buildoptions {
		"/utf-8",
    }

    filter "configurations:Debug*"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release*"
		runtime "Release"
		optimize "on"

    
group "Dependencies"
include "vendor/glfw"
include "vendor/glm"
include "vendor/lz4"
include "vendor/meshoptimizer"
include "vendor/mikktspace"
include "vendor/imgui"
include "vendor/efsw"
include "vendor/catch2"
include "vendor/tracy"

group "Tools"
include "tools/AssetLib"
include "tools/AssetConverter"
include "tools/ShaderBindGroupGen"

group ""
include "CoreLib"
include "VulkanRenderer"

group "Tests"
include "CoreLib/Tests"
include "VulkanRenderer/Tests"