add_rules("mode.release", "mode.debug")
function GetValue(funcOrValue)
    if type(funcOrValue) == 'function' then
        return funcOrValue()
    else
        return funcOrValue
    end
end
function Execute(map, func)
    if map ~= nil then
        func(GetValue(map))
    end
end
function SetException(enableException)
    if (enableException ~= nil) and (GetValue(enableException)) then
        add_cxflags("/EHsc", {
            force = true
        })
    end
end
function BuildProject(config)
    target(GetValue(config.projectName))
    set_languages("clatest")
    set_languages("cxx20")
    projectType = GetValue(config.projectType)
    if projectType ~= nil then
        set_kind(projectType)
    end
    Execute(config.macros, add_defines)
    Execute(config.files, add_files)
    Execute(config.includePaths, add_includedirs)
    Execute(config.depends, add_deps)
    unityBuildBatch = GetValue(config.unityBuildBatch)
    if (unityBuildBatch ~= nil) and (unityBuildBatch > 0) then
        add_rules("c.unity_build", {
            batchsize = unityBuildBatch
        })
        add_rules("c++.unity_build", {
            batchsize = unityBuildBatch
        })
    end
    if is_mode("release") then
        set_optimize("aggressive")
        if is_plat("windows") then
            set_runtimes("MD")
        end
        add_cxflags("/Zi", "/W0", "/MP", "/Ob2", "/Oi", "/Ot", "/Oy", "/GT", "/GF", "/GS-", "/Gy", "/arch:AVX2", "/Gd",
            "/sdl-", "/GL", "/Zc:preprocessor", "/TP", {
                force = true
            })
    else
        set_optimize("none")
        if is_plat("windows") then
            set_runtimes("MDd")
        end
        add_cxflags("/Zi", "/W0", "/MP", "/Ob0", "/Oy-", "/GF", "/GS", "/arch:AVX2", "/TP", "/Gd", "/Zc:preprocessor", {
            force = true
        })
    end
    SetException(config.exception)
end
add_subdirs("src/")
