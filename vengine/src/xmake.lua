BuildBinary = {
    FileRefresher = false,
    FrustumCulling = false,
    TextureTools = false,
    IR = true,
    Vulkan = false
}
rule("copy_to_unity")
after_build(function(target)
    if is_mode("release") then
        local build_path = "$(buildir)/windows/x64/release/"
        local dstPath = "D:/UnityProject/Assets/Plugins/";
        os.cp(build_path .. "/FrustumCulling.dll", dstPath)
        os.cp("bin/mimalloc.dll", dstPath)
        os.cp(build_path .. "/VEngine_DLL.dll", dstPath)
    end
end)
rule("copy_to_build")
after_build(function(target)
    build_path = nil
    if is_mode("release") then
        build_path = "$(buildir)/windows/x64/release/"
    else
        build_path = "$(buildir)/windows/x64/debug/"
    end
    os.cp("bin/*.dll", build_path)
end)

IncludePaths = {"./"}
-- Abseil
--[[
BuildProject({
    projectName = "Abseil",
    projectType = "shared",
    macros = {"_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX", "ABSL_BUILD_DLL"},
    debugMacros = {"_DEBUG"},
    releaseMacros = {"NDEBUG"},
    files = {"abseil/absl/**.cc"},
    includePaths = {"./", "abseil"},
    debugException = true,
    releaseException = true
})
]]

-- VEngine_DLL
BuildProject({
    projectName = "VEngine_DLL",
    projectType = "shared",
    macros = {"COMMON_DLL_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"},
    debugMacros = {"_DEBUG"},
    releaseMacros = {"NDEBUG"},
    files = {"Common/*.cpp", "Utility/*.cpp", "taskflow/src.cpp"},
    includePaths = IncludePaths,
    unityBuildBatch = 4,
    debugException = true,
    releaseException = true
})
if is_plat("windows") then
    add_links("kernel32", "User32", "Gdi32", "Shell32")
end
-- VEngine_Compute
BuildProject({
    projectName = "VEngine_Compute",
    projectType = "shared",
    macros = {"_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"},
    debugMacros = {"_DEBUG"},
    releaseMacros = {"NDEBUG"},
    files = {"Unity/**.cpp"},
    includePaths = IncludePaths,
    depends = {"VEngine_DLL"},
    unityBuildBatch = 4,
    debugException = true,
    releaseException = true
})
-- VEngine_Database
BuildProject({
    projectName = "VEngine_Database",
    projectType = "shared",
    macros = {"VENGINE_DATABASE_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"},
    debugMacros = {"_DEBUG"},
    releaseMacros = {"NDEBUG"},
    files = {"Database/*.cpp"},
    includePaths = IncludePaths,
    depends = {"VEngine_DLL"},
    debugException = true,
    releaseException = true
})
-- STB
BuildProject({
    projectName = "stb",
    projectType = "shared",
    macros = {"STB_EXPORT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"},
    debugMacros = {"_DEBUG"},
    releaseMacros = {"NDEBUG"},
    files = {"stb/*.cpp"},
    depends = {"VEngine_DLL"},
    includePaths = IncludePaths,
    debugException = true,
    releaseException = true
})
-- TextureTools
if BuildBinary.TextureTools == true then
    BuildProject({
        projectName = "TextureTools",
        projectType = "shared",
        macros = {"TEXTOOLS_EXPORT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"},
        debugMacros = {"_DEBUG"},
        releaseMacros = {"NDEBUG"},
        files = {"TextureTools/*.cpp"},
        depends = {"VEngine_DLL", "stb"},
        includePaths = IncludePaths,
        debugException = true,
        releaseException = true
    })
end
-- VEngine_Graphics
BuildProject({
    projectName = "VEngine_Graphics",
    projectType = "shared",
    macros = {"VENGINE_GRAPHICS_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"},
    debugMacros = {"_DEBUG"},
    releaseMacros = {"NDEBUG"},
    files = {"Graphics/**.cpp"},
    includePaths = IncludePaths,
    depends = {"VEngine_DLL"},
    unityBuildBatch = 4,
    debugException = true,
    releaseException = true
})
add_links("lib/dxcompiler")
-- VEngine_DirectX
BuildProject({
    projectName = "VEngine_DirectX",
    projectType = "shared",
    macros = {"VENGINE_DIRECTX_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"},
    debugMacros = {"_DEBUG"},
    releaseMacros = {"NDEBUG"},
    files = {"DirectX/**.cpp"},
    includePaths = IncludePaths,
    depends = {"VEngine_Graphics"},
    unityBuildBatch = 4,
    debugException = true,
    releaseException = true
})
if is_plat("windows") then
    add_links("DXGI", "D3D12")
end
-- VEngine_IR
if BuildBinary.IR == true then
    BuildProject({
        projectName = "VEngine_IR",
        projectType = "binary",
        macros = {"VENGINE_IR_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"},
        debugMacros = {"_DEBUG"},
        releaseMacros = {"NDEBUG"},
        files = {"ir/**.cpp"},
        includePaths = IncludePaths,
        depends = {"VEngine_DLL"},
        debugException = true,
        releaseException = true

    })
end
-- VEngine_Vulkan
if BuildBinary.Vulkan == true then
    BuildProject({
        projectName = "VEngine_Vulkan",
        projectType = "shared",
        macros = {"VENGINE_VULKAN_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"},
        debugMacros = {"_DEBUG"},
        releaseMacros = {"NDEBUG"},
        files = {"VulkanImpl/*.cpp"},
        includePaths = {"./", "C:/VulkanSDK/1.3.204.0/Include/"},
        depends = {"VEngine_DLL"},
        debugException = true,
        releaseException = true
    })
    add_links("C:/VulkanSDK/1.3.204.0/Lib/vulkan-1", "lib/glfw3dll", "lib/glfw3")
    if is_plat("windows") then
        add_links("User32", "kernel32", "Gdi32", "Shell32")
    end
end
-- File refresher
if BuildBinary.FileRefresher == true then
    BuildProject({
        projectName = "FileRefresher",
        projectType = "binary",
        debugMacros = {"_DEBUG"},
        releaseMacros = {"NDEBUG"},
        macros = {"_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"},
        files = {"CPPBuilder/FileRefresher.cpp"},
        includePaths = IncludePaths,
        depends = {"VEngine_DLL"},
        debugException = true,
        releaseException = true
    })
end
if BuildBinary.FrustumCulling == true then
    -- FrustumCulling
    BuildProject({
        projectName = "FrustumCulling",
        projectType = "shared",
        macros = {"_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"},
        debugMacros = {"_DEBUG"},
        releaseMacros = {"NDEBUG"},
        files = {"Renderer/*.cpp"},
        includePaths = IncludePaths,
        depends = {"VEngine_DLL"},
        debugException = true,
        releaseException = true
    })
    add_rules("copy_to_unity")
end

add_rules("copy_to_build")