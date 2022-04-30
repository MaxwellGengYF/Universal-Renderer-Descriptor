BuildBinary = {
    FileRefresher = false,
    FrustumCulling = false,
    IR = true,
    Vulkan = false
}
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
    includePaths = IncludePaths,
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
-- VEngine_Graphics
BuildProject({
    projectName = "VEngine_Graphics",
    projectType = "shared",
    macros = {"VENGINE_GRAPHICS_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"},
    debugMacros = {"_DEBUG"},
    releaseMacros = {"NDEBUG"},
    files = {"Graphics/**.cpp", "stb/*.cpp"},
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
end

after_build(function(target)
    build_path = nil
    if is_mode("release") then
        build_path = "$(buildir)/windows/x64/release/"
    else
        build_path = "$(buildir)/windows/x64/debug/"
    end
    os.cp("bin/*.dll", build_path)
end)
