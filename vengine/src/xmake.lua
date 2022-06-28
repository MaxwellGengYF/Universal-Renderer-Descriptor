-- BuildBinary = 'FileRefresher'
-- BuildBinary = 'FrustumCulling'
-- BuildBinary = 'TextureTools'
-- BuildBinary = 'VEngine_IR'
BuildBinary = 'VEngine_Vulkan'
IncludePaths = {"./"}
VulkanLib = "C:/VulkanSDK/1.3.216.0/"

function ProjFilter(name)
    if BuildBinary == name then
        return name
    end
    return nil
end
function Macro(tab)
    if is_mode("release") then
        table.insert(tab, "NDEBUG")
    else
        table.insert(tab, "_DEBUG")
    end
    return tab
end

rule("copy_to_unity")
after_build(function(target)
    if is_mode("release") then
        local build_path = "$(buildir)/windows/x64/release/"
        local dstPath = "D:/UnityProject/Assets/Plugins/";
        os.cp(build_path .. "/FrustumCulling.dll", dstPath)
        os.cp(build_path .. "/VEngine_DLL.dll", dstPath)
    end
end)
rule("copy_to_build")
after_build(function(target)
    if is_mode("release") then
        os.cp("bin/release/*.dll", "$(buildir)/windows/x64/release/")
    else
        os.cp("bin/debug/*.dll", "$(buildir)/windows/x64/debug/")
    end
end)
rule("copy_shaders")
after_build(function(target)
    local build_path = nil
    if is_mode("release") then
        build_path = "$(buildir)/windows/x64/release/"
    else
        build_path = "$(buildir)/windows/x64/debug/"
    end
    os.cp("shaders/*.compute", build_path)
    os.cp("shaders/*.hlsl", build_path)
end)
rule("copy_glfw")
after_build(function(target)
    local build_path = nil
    if is_mode("release") then
        build_path = "$(buildir)/windows/x64/release/"
    else
        build_path = "$(buildir)/windows/x64/debug/"
    end
    os.cp("src/glfw/glfw3.dll", build_path)
end)

-- Abseil
--[[
BuildProject({
    projectName = "Abseil",
    projectType = "shared",
    macros = function()
        return function({"_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX", "ABSL_BUILD_DLL"})
    end,
    files = {"abseil/absl/**.cc"},
    includePaths = {"./", "abseil"},
    exception = true
})
]]

-- VEngine_DLL
BuildProject({
    projectName = "VEngine_DLL",
    projectType = "shared",
    macros = function()
        return Macro({"COMMON_DLL_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
    end,
    files = {"Common/*.cpp", "Utility/*.cpp", "taskflow/src.cpp"},
    includePaths = IncludePaths,
    unityBuildBatch = 4,
    exception = true,
    rules = "copy_to_build"
})

-- VEngine_Compute
BuildProject({
    projectName = "VEngine_Compute",
    projectType = "shared",
    macros = function()
        return Macro({"_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
    end,
    files = {"Unity/**.cpp"},
    includePaths = IncludePaths,
    depends = "VEngine_DLL",
    unityBuildBatch = 4,
    exception = true
})
-- VEngine_Database
BuildProject({
    projectName = "VEngine_Database",
    projectType = "shared",
    macros = function()
        return Macro({"VENGINE_DATABASE_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
    end,
    files = {"Database/*.cpp"},
    includePaths = IncludePaths,
    depends = "VEngine_DLL",
    exception = true
})
-- STB
BuildProject({
    projectName = "stb",
    projectType = "shared",
    macros = function()
        return Macro({"STB_EXPORT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
    end,
    files = {"stb/*.cpp"},
    depends = "VEngine_DLL",
    includePaths = IncludePaths,
    exception = true
})
-- TextureTools
BuildProject({
    projectName = function()
        return ProjFilter("TextureTools")
    end,
    projectType = "binary",
    macros = function()
        return Macro({"TEXTOOLS_EXPORT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
    end,
    files = {"TextureTools/*.cpp"},
    depends = {"VEngine_DLL", "stb"},
    includePaths = IncludePaths,
    exception = true
})
-- VEngine_Graphics
BuildProject({
    projectName = "VEngine_Graphics",
    projectType = "shared",
    macros = function()
        return Macro({"VENGINE_GRAPHICS_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
    end,
    files = {"Graphics/**.cpp"},
    includePaths = IncludePaths,
    depends = "VEngine_DLL",
    unityBuildBatch = 4,
    exception = true,
    links = "lib/dxcompiler"
})
-- VEngine_DirectX
BuildProject({
    projectName = "VEngine_DirectX",
    projectType = "shared",
    macros = function()
        return Macro({"VENGINE_DIRECTX_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
    end,
    files = {"DirectX/**.cpp"},
    includePaths = IncludePaths,
    depends = {"VEngine_Graphics"},
    unityBuildBatch = 4,
    exception = true,
    links = function()
        if is_plat("windows") then
            return {"DXGI", "D3D12"}
        end
        return nil
    end
})

-- VEngine_IR
BuildProject({
    projectName = function()
        return ProjFilter("VEngine_IR")
    end,
    projectType = "binary",
    macros = function()
        return Macro({"VENGINE_IR_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
    end,
    files = "ir/**.cpp",
    includePaths = IncludePaths,
    depends = "VEngine_DLL",
    exception = true
})

-- File refresher
BuildProject({
    projectName = function()
        return ProjFilter("FileRefresher")
    end,
    projectType = "binary",
    macros = function()
        return Macro({"_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
    end,
    files = {"CPPBuilder/FileRefresher.cpp"},
    includePaths = IncludePaths,
    depends = "VEngine_DLL",
    exception = true
})
-- FrustumCulling
BuildProject({
    projectName = function()
        return ProjFilter("FrustumCulling")
    end,
    projectType = function()
        if is_mode("release") then
            return "shared"
        else
            return "binary"
        end
    end,
    macros = function()
        return Macro({"_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
    end,
    files = "Renderer/*.cpp",
    includePaths = IncludePaths,
    depends = "VEngine_DLL",
    exception = true,
    rules = "copy_to_unity"
})

-- VEngine_Vulkan
BuildProject({
    projectName = function()
        return ProjFilter("VEngine_Vulkan")
    end,
    projectType = "binary",
    macros = function()
        return Macro({"VENGINE_VULKAN_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
    end,
    files = {"vulkan_impl/**.cpp", "dxc/*.cpp"},
    includePaths = {"./"},
    depends = "VEngine_DLL",
    exception = true,
    links = function()
        local tab = {VulkanLib .. "Lib/vulkan-1", "User32", "kernel32", "Shell32"}
        if is_mode("debug") then
            table.insert(tab, "lib/debug/shaderc_shared")
        else
            table.insert(tab, "lib/release/shaderc_shared")
        end
        return tab
    end,
    rules = {"copy_glfw", "copy_shaders"},
    unityBuildBatch = 4
})
