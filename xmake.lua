-- set minimum xmake version
set_xmakever("3.0.0")

includes("lib/commonlibsse-ng")

local PLUGIN_NAME = "TruePoise"
local PLUGIN_VERSION = "0.9.0"

local VIEW_NAME = "TruePoise"

set_project(PLUGIN_NAME)
set_version(PLUGIN_VERSION)
set_license("GPL-3.0")

add_defines("ARPGHUD_ENABLE_MENU=1")

set_languages("c++23")
set_warnings("allextra")
add_defines("UNICODE", "_UNICODE")

set_policy("package.requires_lock", true)

add_rules("mode.release")
--add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- targets
target(PLUGIN_NAME)
    add_deps("commonlibsse-ng")
    set_symbols("debug")
    add_defines("ARPGHUD_MAGIC_GAMEPLAY")

    add_rules("commonlibsse-ng.plugin", {
       name = PLUGIN_NAME,
       author = "StarkMP <discord: starkmp>",
       description = "Poise system and Minimalist HUD for Skyrim"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")

    after_build(function (target)
        local distdir = path.join(os.projectdir(), "dist", PLUGIN_NAME .. "_" .. PLUGIN_VERSION)
        local viewdir = path.join(distdir, "PrismaUI", "views", VIEW_NAME)
        local plugindir = path.join(distdir, "SKSE", "Plugins")
        local fomoddir = path.join(distdir, "fomod")

        os.mkdir(viewdir)
        os.mkdir(plugindir)
        os.mkdir(fomoddir)

        os.cp(path.join(os.projectdir(), "view", "index.html"), viewdir)
        os.cp(path.join(os.projectdir(), "view", "minimal.css"), viewdir)
        os.cp(path.join(os.projectdir(), "view", "minimal.js"), viewdir)
        os.cp(path.join(os.projectdir(), "README.md"), distdir)
        os.cp(path.join(os.projectdir(), "POISE_GUIDE.txt"), distdir)
        os.cp(target:targetfile(), plugindir)
        os.cp(path.join(os.projectdir(), "TruePoise.ini"), plugindir)
        os.cp(path.join(os.projectdir(), "fomod", "info.xml"), fomoddir)

        local symbolfile = target:symbolfile()
        if symbolfile and os.isfile(symbolfile) then
            os.cp(symbolfile, plugindir)
        end

        cprint("${bright green}creating distribution folder: ${clear}dist/%s_%s", PLUGIN_NAME, PLUGIN_VERSION)
    end)

target("arpghud_magic_diagnostic")
    set_default(false)
    set_basename(PLUGIN_NAME)
    set_targetdir(path.join(os.projectdir(), "build", "magic-diagnostic"))
    add_deps("commonlibsse-ng")
    set_symbols("debug")
    add_defines("ARPGHUD_MAGIC_DIAGNOSTICS")
    add_rules("commonlibsse-ng.plugin", {
       name = PLUGIN_NAME,
       author = "StarkMP <discord: starkmp>",
       description = "Poise magic impact diagnostic build"
    })
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
    after_build(function (target)
        local distdir = path.join(os.projectdir(), "dist", PLUGIN_NAME .. "_" .. PLUGIN_VERSION .. "-magic-diagnostic")
        local viewdir = path.join(distdir, "PrismaUI", "views", VIEW_NAME)
        local plugindir = path.join(distdir, "SKSE", "Plugins")
        local fomoddir = path.join(distdir, "fomod")
        os.mkdir(viewdir)
        os.mkdir(plugindir)
        os.mkdir(fomoddir)
        os.cp(path.join(os.projectdir(), "view", "index.html"), viewdir)
        os.cp(path.join(os.projectdir(), "view", "minimal.css"), viewdir)
        os.cp(path.join(os.projectdir(), "view", "minimal.js"), viewdir)
        os.cp(path.join(os.projectdir(), "README.md"), distdir)
        os.cp(path.join(os.projectdir(), "POISE_GUIDE.txt"), distdir)
        os.cp(target:targetfile(), plugindir)
                os.cp(path.join(os.projectdir(), "TruePoise.ini"), plugindir)
        os.cp(path.join(os.projectdir(), "fomod", "info.xml"), fomoddir)
        local symbolfile = target:symbolfile()
        if symbolfile and os.isfile(symbolfile) then
            os.cp(symbolfile, plugindir)
        end
        cprint("${bright green}creating self-contained diagnostic package: ${clear}dist/%s_%s-magic-diagnostic", PLUGIN_NAME, PLUGIN_VERSION)
    end)

target("arpghud_stagger_diagnostic")
    set_default(false)
    set_basename(PLUGIN_NAME)
    set_targetdir(path.join(os.projectdir(), "build", "stagger-diagnostic"))
    add_deps("commonlibsse-ng")
    set_symbols("debug")
    add_defines("ARPGHUD_MAGIC_GAMEPLAY", "ARPGHUD_STAGGER_DIAGNOSTICS")
    add_rules("commonlibsse-ng.plugin", {
       name = PLUGIN_NAME,
       author = "StarkMP <discord: starkmp>",
       description = "Poise stagger source diagnostic build"
    })
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
    after_build(function (target)
        local distdir = path.join(os.projectdir(), "dist", PLUGIN_NAME .. "_" .. PLUGIN_VERSION .. "-stagger-diagnostic")
        local viewdir = path.join(distdir, "PrismaUI", "views", VIEW_NAME)
        local plugindir = path.join(distdir, "SKSE", "Plugins")
        local fomoddir = path.join(distdir, "fomod")
        os.mkdir(viewdir)
        os.mkdir(plugindir)
        os.mkdir(fomoddir)
        os.cp(path.join(os.projectdir(), "view", "index.html"), viewdir)
        os.cp(path.join(os.projectdir(), "view", "minimal.css"), viewdir)
        os.cp(path.join(os.projectdir(), "view", "minimal.js"), viewdir)
        os.cp(path.join(os.projectdir(), "README.md"), distdir)
        os.cp(path.join(os.projectdir(), "POISE_GUIDE.txt"), distdir)
        os.cp(target:targetfile(), plugindir)
                os.cp(path.join(os.projectdir(), "TruePoise.ini"), plugindir)
        os.cp(path.join(os.projectdir(), "fomod", "info.xml"), fomoddir)
        local symbolfile = target:symbolfile()
        if symbolfile and os.isfile(symbolfile) then
            os.cp(symbolfile, plugindir)
        end
        cprint("${bright green}creating self-contained diagnostic package: ${clear}dist/%s_%s-stagger-diagnostic", PLUGIN_NAME, PLUGIN_VERSION)
    end)

target("arpghud_rogue_stagger_diagnostic")
    set_default(false)
    set_basename(PLUGIN_NAME)
    set_targetdir(path.join(os.projectdir(), "build-rogue-diag"))
    add_deps("commonlibsse-ng")
    set_symbols("debug")
    add_defines("ARPGHUD_MAGIC_GAMEPLAY", "ARPGHUD_ROGUE_STAGGER_DIAGNOSTICS")
    add_rules("commonlibsse-ng.plugin", {
       name = PLUGIN_NAME,
       author = "StarkMP <discord: starkmp>",
       description = "Uncapped rogue stagger diagnostic build"
    })
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
    add_syslinks("shell32", "ole32")
    after_build(function (target)
        local plugindir = path.join(os.projectdir(), "dist", "TruePoise_Diagnostic", "SKSE", "Plugins")
        os.mkdir(plugindir)
        os.cp(target:targetfile(), plugindir)
        local symbolfile = target:symbolfile()
        if symbolfile and os.isfile(symbolfile) then
            os.cp(symbolfile, plugindir)
        end
        cprint("${bright green}diagnostic DLL: ${clear}dist/TruePoise_Diagnostic/SKSE/Plugins/TruePoise.dll")
    end)

target("arpghud_gameplay_candidate")
    set_default(false)
    set_basename(PLUGIN_NAME)
    set_targetdir(path.join(os.projectdir(), "build", "gameplay-candidate"))
    add_deps("commonlibsse-ng")
    set_symbols("debug")
    add_defines("ARPGHUD_MAGIC_GAMEPLAY")
    add_rules("commonlibsse-ng.plugin", {
       name = PLUGIN_NAME,
       author = "StarkMP <discord: starkmp>",
       description = "Poise system experimental gameplay candidate"
    })
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
    after_build(function (target)
        local distdir = path.join(os.projectdir(), "dist", PLUGIN_NAME .. "_" .. PLUGIN_VERSION .. "-candidate")
        local viewdir = path.join(distdir, "PrismaUI", "views", VIEW_NAME)
        local plugindir = path.join(distdir, "SKSE", "Plugins")
        local fomoddir = path.join(distdir, "fomod")
        os.mkdir(viewdir)
        os.mkdir(plugindir)
        os.mkdir(fomoddir)
        os.cp(path.join(os.projectdir(), "view", "index.html"), viewdir)
        os.cp(path.join(os.projectdir(), "view", "minimal.css"), viewdir)
        os.cp(path.join(os.projectdir(), "view", "minimal.js"), viewdir)
        os.cp(path.join(os.projectdir(), "README.md"), distdir)
        os.cp(path.join(os.projectdir(), "POISE_GUIDE.txt"), distdir)
        os.cp(target:targetfile(), plugindir)
                os.cp(path.join(os.projectdir(), "TruePoise.ini"), plugindir)
        os.cp(path.join(os.projectdir(), "fomod", "*"), fomoddir)
        local symbolfile = target:symbolfile()
        if symbolfile and os.isfile(symbolfile) then
            os.cp(symbolfile, plugindir)
        end
        cprint("${bright green}creating experimental candidate: ${clear}dist/%s_%s-candidate", PLUGIN_NAME, PLUGIN_VERSION)
    end)
