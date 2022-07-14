-- BuildBinary = 'VEngine_Compute'
-- BuildBinary = 'FileRefresher'
BuildBinary = 'RecreateRendering'
-- BuildBinary = 'TextureTools'
-- BuildBinary = 'VEngine_IR'
-- BuildBinary = 'VEngine_Vulkan'
-- BuildBinary = "fsr2"
VulkanLib = "C:/VulkanSDK/1.3.216.0/"

function ProjFilter(name)
    if BuildBinary == name then
        return name
    end
    return nil
end

rule("copy_to_unity")
after_build(function(target)
    local build_path = "$(buildir)/windows/x64/release/"
    local dstPath = "D:/UnityProject/Assets/Plugins/";
    os.cp(build_path .. "/RecreateRendering.dll", dstPath)
    os.cp(build_path .. "/VEngine_DLL.dll", dstPath)
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
    privateDefines = function()
        return function({"_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX", "ABSL_BUILD_DLL"})
    end,
    files = {"abseil/absl/**.cc"},
    privateIncludePaths = {"./", "abseil"},
    exception = true
})
]]

-- VEngine_DLL
BuildProject({
    projectName = "VEngine_DLL",
    projectType = "shared",
    event = function()
        add_defines("_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "NOMINMAX", {
            public = true
        })
        add_defines("COMMON_DLL_PROJECT")
        add_files("Common/**.cpp", "Utility/**.cpp", "taskflow/src.cpp")
        add_includedirs("./", {
            public = true
        })
        add_rules("copy_to_build")
    end,
    releaseEvent = function()
        add_defines("NDEBUG", {
            public = true
        })
    end,
    debugEvent = function()
        add_defines("_DEBUG", "DEBUG", {
            public = true
        })
    end,
    unityBuildBatch = 4,
    exception = true
})

-- VEngine_Compute
BuildProject({
    projectName = "VEngine_Compute",
    projectType = "shared",
    event = function()
        add_files("ShaderVariantCull/**.cpp")
        add_deps("VEngine_DLL")
    end,
    exception = true
})
BuildProject({
    projectName = "VEngine_UnityNative",
    projectType = "shared",
    event = function()
        add_files("Unity/**.cpp")
        add_deps("VEngine_DLL")
        add_defines("VENGINE_UNITY_NATIVE")
    end,
    exception = true
})
-- VEngine_Database
BuildProject({
    projectName = "VEngine_Database",
    projectType = "shared",
    event = function()
        add_files("Database/**.cpp")
        add_deps("VEngine_DLL")
        add_defines("VENGINE_DATABASE_PROJECT")
    end,
    exception = true
})
-- STB
BuildProject({
    projectName = "stb",
    projectType = "shared",
    event = function()
        add_files("stb/**.cpp")
        add_deps("VEngine_DLL")
        add_defines("STB_EXPORT")
    end,
    exception = true
})
-- TextureTools
BuildProject({
    projectName = function()
        return ProjFilter("TextureTools")
    end,
    projectType = "binary",
    event = function()
        add_files("TextureTools/**.cpp")
        add_deps("VEngine_DLL", "stb")
        add_defines("TEXTOOLS_EXPORT")
    end,
    exception = true
})
-- VEngine_Graphics
BuildProject({
    projectName = "VEngine_Graphics",
    projectType = "shared",
    event = function()
        add_files("Graphics/**.cpp")
        add_deps("VEngine_DLL")
        add_defines("VENGINE_GRAPHICS_PROJECT")
        add_links("lib/dxcompiler")
    end,
    unityBuildBatch = 4,
    exception = true
})
-- VEngine_DirectX
BuildProject({
    projectName = "VEngine_DirectX",
    projectType = "shared",
    event = function()
        add_files("DirectX/**.cpp")
        add_deps("VEngine_Graphics")
        add_defines("VENGINE_DIRECTX_PROJECT")
        add_links("DXGI", "D3D12")

    end,
    unityBuildBatch = 4,
    exception = true
})
-- VEngine_IR
BuildProject({
    projectName = function()
        return ProjFilter("VEngine_IR")
    end,
    projectType = "binary",
    event = function()
        add_files("ir/**.cpp")
        add_deps("VEngine_DLL")
        add_defines("VENGINE_IR_PROJECT")
    end,
    unityBuildBatch = 4,
    exception = true
})
-- File refresher
BuildProject({
    projectName = function()
        return ProjFilter("FileRefresher")
    end,
    projectType = "binary",
    event = function()
        add_files("CPPBuilder/FileRefresher.cpp")
        add_deps("VEngine_DLL")
    end,
    exception = true
})
-- RecreateRendering
BuildProject({
    projectName = function()
        return ProjFilter("RecreateRendering")
    end,
    projectType = function()
        if is_mode("release") then
            return "shared"
        else
            return "binary"
        end
    end,
    event = function()
        add_files("Renderer/**.cpp")
        add_deps("VEngine_UnityNative")
    end,
    releaseEvent = function()
        add_rules("copy_to_unity")
    end,
    exception = true
})

-- VEngine_Vulkan
BuildProject({
    projectName = function()
        return ProjFilter("VEngine_Vulkan")
    end,
    projectType = "binary",
    event = function()
        add_files("vulkan_impl/**.cpp", "dxc/**.cpp")
        add_includedirs("./vulkan_impl")
        add_deps("VEngine_DLL")
        add_rules("copy_glfw", "copy_shaders")
        add_links(VulkanLib .. "Lib/vulkan-1", "User32", "kernel32", "Shell32")
    end,
    releaseEvent = function()
        add_links("lib/release/shaderc_shared")
    end,
    debugEvent = function()
        add_links("lib/debug/shaderc_shared")
    end,
    unityBuildBatch = 4,
    exception = true
})
BuildProject({
    projectName = function()
        return ProjFilter("fsr2")
    end,
    projectType = "shared",

    event = function()
        add_defines("FFX_CPU")
        add_files("AMD_FSR2/**.cpp")
        add_includedirs("AMD_FSR2/")
        add_deps("VEngine_UnityNative")
        after_build(function(target)
            local build_path = nil
            if is_mode("release") then
                build_path = "$(buildir)/windows/x64/release/"
            else
                build_path = "$(buildir)/windows/x64/debug/"
            end
            local dstPath = "D:/UnityProject/Assets/Plugins/";
            os.cp(build_path .. "/fsr2.dll", dstPath)
            os.cp(build_path .. "/VEngine_DLL.dll", dstPath)
            os.cp(build_path .. "/VEngine_UnityNative.dll", dstPath)
        end)
    end,
    exception = true
})
