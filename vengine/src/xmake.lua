-- Abseil
IncludePaths = {"./", "abseil"}
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

-- VEngine_DLL
BuildProject({
    projectName = "VEngine_DLL",
    projectType = "shared",
    macros = {"COMMON_DLL_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"},
    debugMacros = {"_DEBUG"},
    releaseMacros = {"NDEBUG"},
    files = {"Common/*.cpp", "Utility/*.cpp", "taskflow/src.cpp"},
    includePaths = IncludePaths,
    depends = {"Abseil"},
    unityBuildBatch = 4,
    debugException = true,
    releaseException = true
})
if is_plat("windows") then
    add_links("kernel32", "User32", "Gdi32", "Shell32")
end

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
BuildProject({
    projectName = "VEngine_IR",
    projectType = "shared",
    macros = {"VENGINE_IR_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"},
    debugMacros = {"_DEBUG"},
    releaseMacros = {"NDEBUG"},
    files = {"ir/*.cpp"},
    includePaths = IncludePaths,
    depends = {"VEngine_DLL"},
    debugException = true,
    releaseException = true

})
-- VEngine_Vulkan
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
--[[
-- File refresher
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
after_build(function(target)
    if not is_mode("release") then
        return
    end
    build_path = "$(buildir)/windows/x64/release/"
    os.cp("bin/*.dll", build_path)
end)
]]

-- FrustumCulling
BuildProject({
    projectName = "FrustumCulling",
    projectType = "binary",
    macros = {"_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"},
    debugMacros = {"_DEBUG"},
    releaseMacros = {"NDEBUG"},
    files = {"Renderer/*.cpp"},
    includePaths = IncludePaths,
    depends = {"VEngine_DLL"},
    debugException = true,
    releaseException = true
})

after_build(function(target)
    if not is_mode("release") then
        return
    end
    build_path = "$(buildir)/windows/x64/release/"
    os.cp("bin/*.dll", build_path)
    -- os.cp(build_path .. "FrustumCulling.dll", "D:/UnityProject/Assets/Plugins")
    -- os.cp(build_path .. "VEngine_DLL.dll", "D:/UnityProject/Assets/Plugins")
end)
