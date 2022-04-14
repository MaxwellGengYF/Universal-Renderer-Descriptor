{
    projectName = "name",
    projectType = "shared", -- shared or binary?(dll or exe)
    macros = {"PROJECT"},
    debugMacros =  {"_DEBUG"},
    releaseMacros = {"NDEBUG"},
    depends = {"another_project"},
    files = {"src/**.cpp"},
    includePaths = {"./"},
    unityBuildBatch = 0,
    end,
}
add_link(a,b,c)
after_build(function(target) 
end)