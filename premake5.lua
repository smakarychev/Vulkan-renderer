include "dependencies.lua"

workspace "VulkanRenderer"
    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
    configurations { "Debug", "Release"}
    architecture "x86_64"
    editandcontinue "Off"
    flags
	{
		"MultiProcessorCompile"
	}
    startproject "VulkanRenderer"

    linkoptions { "/NODEFAULTLIB:LIBCMTD.LIB" }

    
group "Dependencies"
include "vendor/glfw"
include "vendor/glm"
include "vendor/lz4"
include "vendor/meshoptimizer"
include "vendor/imgui"
include "vendor/efsw"

group "Tools"
include "tools/AssetLib"
include "tools/AssetConverter"

group ""
include "VulkanRenderer"