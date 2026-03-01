project "mikktspace"
    kind "StaticLib"
    language "C"
    warnings "Off"  
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    
    files
    {
        "src/**",
    }