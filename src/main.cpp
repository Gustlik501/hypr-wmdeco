// SPDX-License-Identifier: GPL-2.0-or-later
// Based on hyprbars v0.57.0, Copyright (c) 2023 Hypr Development (BSD-3-Clause).
#include "wmdeco.hpp"
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/view/window/Window.hpp>
#include <hyprland/src/desktop/view/window/WindowPresentation.hpp>
#include <hyprland/src/desktop/rule/windowRule/WindowRuleEffectContainer.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/state/MonitorState.hpp>
extern "C" {
#include <lua.h>
}
using namespace WM;
using namespace Config::Values;
constexpr auto REFRESH = Config::Supplementary::REFRESH_LAYOUTS;

// IValue keeps const char* names, so their storage must outlive the values.
static const char* name(const std::string& key) {
    return configNames.emplace_back("plugin:wmdeco:" + key).c_str();
}
static SP<CIntValue> integer(const std::string& key, int def, int min, int max) {
    auto v = makeShared<CIntValue>(name(key), "wmdeco geometry/font setting", def, SIntValueOptions{.min = min, .max = max, .refresh = REFRESH});
    HyprlandAPI::addConfigValueV2(handle, v);
    return v;
}
static SP<CStringValue> string(const std::string& key, const std::string& def, std::vector<std::string> choices = {}) {
    SStringValueOptions opts{.refresh = REFRESH};
    if (!choices.empty()) opts.validator = strChoice(std::move(choices));
    auto v = makeShared<CStringValue>(name(key), "wmdeco texture/font/action setting", def, std::move(opts));
    HyprlandAPI::addConfigValueV2(handle, v);
    return v;
}
static SP<CColorValue> color(const std::string& key, uint32_t def) {
    auto v = makeShared<CColorValue>(name(key), "wmdeco opaque ARGB color", def, SColorValueOptions{.refresh = REFRESH});
    HyprlandAPI::addConfigValueV2(handle, v);
    return v;
}
static TextureConfig tex(const std::string& key, uint32_t def, bool inherit = false) {
    return {string(key + "_texture", inherit ? "inherit" : "solid", inherit ? std::vector<std::string>{"inherit", "solid", "hgradient", "vgradient", "dgradient"} : std::vector<std::string>{"solid", "hgradient", "vgradient", "dgradient"}),
            color(key + "_color1", def), color(key + "_color2", def)};
}
int WM::titleHeight() { return cfg.height->value() ? cfg.height->value() : cfg.autoHeight; }
int WM::buttonSize() { return std::min<int>(cfg.buttonSize->value() ? cfg.buttonSize->value() : titleHeight(), titleHeight()); }
Texture WM::texture(const TextureConfig& t) { return {t.type->value(), uint32_t(t.color1->value()), uint32_t(t.color2->value())}; }

void WM::refresh() {
    ++epoch;
    cfg.autoHeight = fontHeight() + 8; // WMFontHeight + 2 * TITLEBAR_EXTEND_SPACE
    for (auto& d : decos) if (d) d->sync();
    for (auto& m : State::monitorState()->monitors()) {
        m->m_scheduledRecalc = true;
        g_layoutManager->recalculateMonitor(m);
        g_pHyprRenderer->damageMonitor(m);
    }
}
static void addWindow(PHLWINDOW w) {
    if (!validMapped(w)) return;
    if (std::ranges::any_of(w->presentation().decorations(), [](const auto& d) { return d->getDisplayName() == "WMTitlebar"; })) return;
    for (bool bottom : {false, true}) {
        auto d = makeShared<Deco>(w, bottom);
        decos.emplace_back(d);
        HyprlandAPI::addWindowDecoration(handle, w, d);
        d->sync();
    }
}
// Use the public Lua config API, including its refresh scheduling, not raw memory writes.
static int setEnabled(lua_State* L, bool enabled) {
    lua_getglobal(L, "hl");
    lua_getfield(L, -1, "config");
    lua_newtable(L); lua_newtable(L); lua_newtable(L);
    lua_pushboolean(L, enabled); lua_setfield(L, -2, "enabled");
    lua_setfield(L, -2, "wmdeco"); lua_setfield(L, -2, "plugin");
    lua_call(L, 1, 0);
    lua_pop(L, 1);
    return 0;
}
static int enable(lua_State* L) { return setEnabled(L, true); }
static int disable(lua_State* L) { return setEnabled(L, false); }
static int toggle(lua_State* L) { return setEnabled(L, !cfg.enabled->value()); }
static int stats(lua_State* L) {
    lua_newtable(L);
    lua_pushinteger(L, paintCount); lua_setfield(L, -2, "paint_count");
    lua_pushinteger(L, renderCount); lua_setfield(L, -2, "render_count");
    lua_pushinteger(L, titleHeight()); lua_setfield(L, -2, "title_height");
    lua_pushboolean(L, cfg.enabled->value()); lua_setfield(L, -2, "enabled");
    return 1;
}

APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }
APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE h) {
    handle = h;
    if (std::string_view(__hyprland_api_get_hash()) != __hyprland_api_get_client_hash())
        throw std::runtime_error("wmdeco: Hyprland header/runtime hash mismatch");
    cfg.enabled = makeShared<CBoolValue>(name("enabled"), "Enable Window Maker decorations live", true, SBoolValueOptions{.refresh = REFRESH});
    HyprlandAPI::addConfigValueV2(handle, cfg.enabled);
    cfg.font = string("font_family", "Helvetica");
    cfg.fontSize = integer("font_size", 12, 1, 96);
    cfg.fontWeight = integer("font_weight", 700, 100, 1000);
    cfg.height = integer("title_height", 0, 0, 256);
    cfg.buttonSize = integer("button_size", 0, 0, 256);
    cfg.buttonPadding = integer("button_padding", 0, 0, 64);
    cfg.titlePadding = integer("title_padding", 6, 0, 128);
    cfg.justify = string("title_justify", "center", {"left", "center", "right"});
    cfg.resizeHeight = integer("resize_height", 8, 0, 128);
    cfg.cornerWidth = integer("resize_corner_width", 28, 0, 512);
    cfg.doubleClickMs = integer("double_click_ms", 250, 0, 2000);
    cfg.excludeTags = string("exclude_tags", "games");
    cfg.minimize = string("minimize_dispatcher", "hl.dsp.window.move({ workspace = \"special:minimized\", follow = false })");
    cfg.doubleClick = string("double_click_dispatcher", "hl.dsp.window.fullscreen({ mode = \"maximized\", action = \"toggle\" })");
    const std::array<std::string, 3> states{"focused", "unfocused", "owner"};
    const std::array<uint32_t, 3> bg{0xff000000, 0xffaaaaaa, 0xff666666};
    for (int i = 0; i < 3; ++i) {
        cfg.title[i] = tex(states[i], bg[i]);
        cfg.button[i] = tex(states[i] + "_button", bg[i], true);
        cfg.text[i] = color(states[i] + "_text_color", i == 1 ? 0xff000000 : 0xffffffff);
    }
    cfg.resize = tex("resize", 0xffaaaaaa);
    noBarRule = Desktop::Rule::windowEffects()->registerEffect("wmdeco:no_bar");
    HyprlandAPI::addLuaFunction(handle, "wmdeco", "enable", enable);
    HyprlandAPI::addLuaFunction(handle, "wmdeco", "disable", disable);
    HyprlandAPI::addLuaFunction(handle, "wmdeco", "toggle", toggle);
    HyprlandAPI::addLuaFunction(handle, "wmdeco", "stats", stats);
    auto& ev = Event::bus()->m_events;
    listeners.emplace_back(ev.window.open.listen([](PHLWINDOW w) { addWindow(w); }));
    listeners.emplace_back(ev.window.updateRules.listen([](PHLWINDOW w) {
        for (auto& d : decos) if (d && d->owner() == w) d->sync();
    }));
    listeners.emplace_back(ev.window.fullscreen.listen([](PHLWINDOW w) {
        for (auto& d : decos) if (d && d->owner() == w) d->sync();
    }));
    listeners.emplace_back(ev.window.title.listen([](PHLWINDOW w) {
        for (auto& d : decos) if (d && d->owner() == w && d->visible()) d->damageEntire();
    }));
    listeners.emplace_back(ev.window.active.listen([](PHLWINDOW, Desktop::eFocusReason) {
        for (auto& d : decos) if (d && d->visible()) d->damageEntire();
    }));
    listeners.emplace_back(ev.config.reloaded.listen([] { refresh(); }));
    listeners.emplace_back(ev.config.props_refreshed.listen([](bool) { refresh(); }));
    cfg.autoHeight = fontHeight() + 8;
    for (auto& w : Desktop::windowState()->windows()) addWindow(w);
    HyprlandAPI::reloadConfig();
    return {"wmdeco", "Window Maker titlebars and resize handles", "wmdeco contributors; based on Vaxry's hyprbars", "0.1.0"};
}
APICALL EXPORT void PLUGIN_EXIT() {
    listeners.clear();
    g_pHyprRenderer->context().m_pass.removeAllOfType("WMDecoPass");
    Desktop::Rule::windowEffects()->unregisterEffect(noBarRule);
    for (auto& m : State::monitorState()->monitors()) m->m_scheduledRecalc = true;
}
