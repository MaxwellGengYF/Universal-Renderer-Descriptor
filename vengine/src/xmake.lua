BuildBinary = {
    FileRefresher = false,
    FrustumCulling = true,
    TextureTools = false,
    IR = false,
    Vulkan = false
}
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
    exception = true
})
if is_plat("windows") then
    add_links("kernel32", "User32", "Gdi32", "Shell32")
end
-- VEngine_Compute
BuildProject({
    projectName = "VEngine_Compute",
    projectType = "shared",
    macros = function()
        return Macro({"_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
    end,
    files = {"Unity/**.cpp"},
    includePaths = IncludePaths,
    depends = {"VEngine_DLL"},
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
    depends = {"VEngine_DLL"},
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
    depends = {"VEngine_DLL"},
    includePaths = IncludePaths,
    exception = true
})
-- TextureTools
if BuildBinary.TextureTools == true then
    BuildProject({
        projectName = "TextureTools",
        projectType = "shared",
        macros = function()
            return Macro({"TEXTOOLS_EXPORT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
        end,
        files = {"TextureTools/*.cpp"},
        depends = {"VEngine_DLL", "stb"},
        includePaths = IncludePaths,
        exception = true
    })
end
-- VEngine_Graphics
BuildProject({
    projectName = "VEngine_Graphics",
    projectType = "shared",
    macros = function()
        return Macro({"VENGINE_GRAPHICS_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
    end,
    files = {"Graphics/**.cpp"},
    includePaths = IncludePaths,
    depends = {"VEngine_DLL"},
    unityBuildBatch = 4,
    exception = true
})
add_links("lib/dxcompiler")
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
    exception = true
})
if is_plat("windows") then
    add_links("DXGI", "D3D12")
end
-- VEngine_IR
if BuildBinary.IR == true then
    BuildProject({
        projectName = "VEngine_IR",
        projectType = "binary",
        macros = function()
            return Macro({"VENGINE_IR_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
        end,
        files = {"ir/**.cpp"},
        includePaths = IncludePaths,
        depends = {"VEngine_DLL"},
        exception = true
    })
end
-- VEngine_Vulkan
if BuildBinary.Vulkan == true then
    BuildProject({
        projectName = "VEngine_Vulkan",
        projectType = "shared",
        macros = function()
            return Macro({"VENGINE_VULKAN_PROJECT", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
        end,
        files = {"VulkanImpl/*.cpp"},
        includePaths = {"./", "C:/VulkanSDK/1.3.204.0/Include/"},
        depends = {"VEngine_DLL"},
        exception = true
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
        macros = function()
            return Macro({"_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX"})
        end,
        files = {"CPPBuilder/FileRefresher.cpp"},
        includePaths = IncludePaths,
        depends = {"VEngine_DLL"},
        exception = true
    })
end
if BuildBinary.FrustumCulling == true then
    -- FrustumCulling
    BuildProject({
        projectName = "FrustumCulling",
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
        files = {"Renderer/*.cpp"},
        includePaths = IncludePaths,
        depends = {"VEngine_DLL"},
        exception = true
    })
    add_rules("copy_to_unity")
end

add_rules("copy_to_build")
