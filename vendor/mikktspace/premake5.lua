project "mikktspace"
    kind "StaticLib"
        language "C"
        targetdir ("bin/" .. outputdir .. "/%{prj.name}")
        objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
        
        files
        {
            "src/**",
        }