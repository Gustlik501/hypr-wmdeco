// SPDX-License-Identifier: GPL-2.0-or-later
// Positioning, input and render-pass integration adapted from hyprbars v0.57.0.
#include "wmdeco.hpp"
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/state/LayerState.hpp>
#include <hyprland/src/desktop/state/ViewHitTester.hpp>
#include <hyprland/src/desktop/view/window/Window.hpp>
#include <hyprland/src/desktop/view/window/WindowBackend.hpp>
#include <hyprland/src/desktop/view/window/WindowPresentation.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/protocols/LayerShell.hpp>
#include <drm_fourcc.h>
#include <linux/input-event-codes.h>
#include <sstream>
#include <cmath>

using namespace WM;
using namespace Render::GL;

namespace {
class WMDecoPass : public IPassElement {
    Deco* deco;
    float alpha;
  public:
    WMDecoPass(Deco* d, float a) : deco(d), alpha(a) {}
    std::vector<UP<IPassElement>> draw(Render::CRenderContext& ctx) override {
        deco->render(ctx, ctx.m_data.pMonitor.lock(), alpha);
        return {};
    }
    bool needsLiveBlur(Render::CRenderContext&) override { return false; }
    bool needsPrecomputeBlur(Render::CRenderContext&) override { return false; }
    const char* passName() override { return "WMDecoPass"; }
    ePassElementType type() override { return EK_CUSTOM; }
    std::optional<CBox> boundingBox(Render::CRenderContext& ctx) override {
        return deco->box().translate(-ctx.m_data.pMonitor->m_position).expand(1);
    }
};
bool inside(Vector2D p, const CBox& b) { return p.x >= b.x && p.y >= b.y && p.x < b.x+b.w && p.y < b.y+b.h; }
int cornerWidth(double width) { return std::max(0, std::min<int>(cfg.cornerWidth->value(), (width-20)/2)); }
}
Deco::Deco(PHLWINDOW w, bool b) : IHyprWindowDecoration(w), window(w), bottom(b) {
    mouseButton = Event::bus()->m_events.input.mouse.button.listen([this](IPointer::SButtonEvent e, Event::SCallbackInfo& i) { button(e, i); });
    mouseMove = Event::bus()->m_events.input.mouse.move.listen([this](Vector2D p, Event::SCallbackInfo&) { motion(p); });
}
Deco::~Deco() {
    cancelGrab();
    std::erase_if(decos, [this](const auto& d) { return !d || d.get() == this; });
}
bool Deco::visible() const {
    auto w = window.lock();
    return w && w->mapped() && !hidden && cfg.enabled->value() &&
        Fullscreen::controller()->getFullscreenModes(w).internal != Fullscreen::FSMODE_FULLSCREEN;
}
SDecorationPositioningInfo Deco::getPositioningInfo() {
    SDecorationPositioningInfo info;
    const int height = visible() ? (bottom ? int(cfg.resizeHeight->value()) : titleHeight()) : 0;
    info.policy = height ? DECORATION_POSITION_STICKY : DECORATION_POSITION_ABSOLUTE;
    info.edges = bottom ? DECORATION_EDGE_BOTTOM : DECORATION_EDGE_TOP;
    info.priority = 10005; // include bars inside Hyprland's own border
    info.reserved = true;
    info.desiredExtents = bottom ? SBoxExtents{{}, {0,height}} : SBoxExtents{{0,height}, {}};
    return info;
}
void Deco::onPositioningReply(const SDecorationPositioningReply& reply) { assigned = reply.assignedGeometry; }
CBox Deco::box() {
    if (!validMapped(window)) return {};
    auto w = window.lock();
    CBox b = assigned;
    b.translate(g_pDecorationPositioner->getEdgeDefinedPoint(bottom ? DECORATION_EDGE_BOTTOM : DECORATION_EDGE_TOP, w));
    auto ws = w->m_workspace;
    if (ws && !(w->m_state & Desktop::View::WINDOW_STATE_PINNED)) b.translate(ws->m_renderOffset->value());
    return b;
}
void Deco::damageEntire() { if (validMapped(window)) g_pHyprRenderer->damageBox(box()); }
void Deco::sync() {
    auto w = window.lock();
    if (!w) return;
    damageEntire();
    hidden = !w->m_ruleApplicator->decorate().valueOrDefault();
    auto& props = w->m_ruleApplicator->m_otherProps.props;
    if (props.contains(noBarRule)) hidden |= truthy(props.at(noBarRule)->effect);
    std::string tags = cfg.excludeTags->value();
    std::replace(tags.begin(), tags.end(), ',', ' ');
    std::istringstream stream(tags);
    for (std::string tag; stream >> tag;) hidden |= w->m_ruleApplicator->m_tagKeeper.isTagged(tag);
    if (!visible()) cancelGrab();
    // Config refresh advances epoch; rules only change visibility, not the pixels.
    g_pDecorationPositioner->repositionDeco(this);
    w->updateWindowData();
    if (auto t = w->layoutTarget()) t->recalc();
    damageEntire();
}
void Deco::updateWindow(PHLWINDOW) { if (visible()) damageEntire(); }
void Deco::draw(Render::CRenderContext& ctx, PHLMONITOR, const float& a, const Render::SWindowRenderPresentation&) {
    if (!visible() || (bottom && !cfg.resizeHeight->value())) return;
    Render::IHyprRenderer::addPassElement(ctx, makeUnique<WMDecoPass>(this, a));
}
void Deco::render(Render::CRenderContext& ctx, PHLMONITOR mon, float a) {
    if (!visible()) return;
    auto w = window.lock();
    auto logical = box();
    int width = std::round(logical.w), height = std::round(logical.h);
    if (width < 1 || height < 1 || width*mon->m_scale > 32768) return;
    ++renderCount;
    int state = 0;
    if (!bottom) {
        auto focused = Desktop::focusState()->window();
        state = focused == w ? 0 : 1;
        // P means the owner/ancestor of the focused transient, not every app window.
        auto parent = focused ? focused->backend().parent() : nullptr;
        for (int depth = 0; parent && depth < 64; ++depth, parent = parent->backend().parent()) {
            if (parent == w) { state = 2; break; }
        }
    }
    static const std::string EMPTY;
    const std::string& title = bottom ? EMPTY : w->metadata().title();
    if (!cached || lastWidth != width || lastHeight != height || lastScale != mon->m_scale || lastEpoch != epoch ||
        (!bottom && (state != lastState || title != lastTitle || pressed != lastPressed))) {
        PaintSpec s{.bottom = bottom, .width = width, .height = height, .button = buttonSize(), .padding = int(cfg.buttonPadding->value()),
            .titlePadding = int(cfg.titlePadding->value()), .corner = cornerWidth(width), .pressed = pressed, .scale = mon->m_scale,
            .texture = texture(bottom ? cfg.resize : cfg.title[state]), .buttonTexture = texture(cfg.button[state]),
            .textColor = uint32_t(cfg.text[state]->value()), .title = title, .font = cfg.font->value(), .justify = cfg.justify->value(),
            .fontSize = int(cfg.fontSize->value()), .weight = int(cfg.fontWeight->value())};
        ++paintCount;
        auto surf = paint(s);
        cached = makeShared<CGLTexture>(DRM_FORMAT_ARGB8888, cairo_image_surface_get_data(surf), cairo_image_surface_get_stride(surf),
            Vector2D(cairo_image_surface_get_width(surf), cairo_image_surface_get_height(surf)), false, true);
        cairo_surface_destroy(surf);
        lastWidth = width; lastHeight = height; lastScale = mon->m_scale; lastEpoch = epoch;
        lastState = state; lastTitle = title; lastPressed = pressed;
    }
    logical.translate(w->presentation().floatingOffset());
    CBox b = logical;
    b.translate(-mon->m_position).scale(mon->m_scale).round();
    CHyprOpenGLImpl::STextureRenderData data;
    data.a = a;
    g_pHyprOpenGL->renderTexture(ctx, cached, b, data);
}
int Deco::hitButton(Vector2D p) const {
    const int size = buttonSize(), pad = cfg.buttonPadding->value();
    double y = (assigned.h-size)/2;
    if (p.y < y || p.y >= y+size) return 0;
    if (assigned.w >= 4*assigned.h && p.x >= pad && p.x < pad+size) return 1;
    if (assigned.w >= 2*assigned.h && p.x >= assigned.w-pad-size && p.x < assigned.w-pad) return 2;
    return 0;
}
bool Deco::acceptsInput(Vector2D p) {
    if (!visible() || !inside(p, box())) return false;
    auto w = window.lock();
    if (g_pSeatManager->m_seatGrab && !g_pSeatManager->m_seatGrab->accepts(w->wlSurface()->resource())) return false;
    if (w->hasPopupAt(p)) return false;
    Desktop::CViewHitTester hit{*Desktop::viewState()};
    if (hit.windowAt(p, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING) != w) return false;
    auto mon = Desktop::focusState()->monitor();
    if (!mon) return false;
    PHLLS found;
    Vector2D coords;
    for (auto layer : {ZWLR_LAYER_SHELL_V1_LAYER_TOP, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY}) {
        found = nullptr;
        hit.layerSurfaceAt(p, &mon->m_layerSurfaceLayers[layer], &coords, &found);
        if (found) return false;
    }
    return true;
}
void Deco::cancelGrab() {
    if (dragging) g_layoutManager->endDragTarget();
    dragging = pending = captured = false;
    pressed = 0;
}
void Deco::button(IPointer::SButtonEvent e, Event::SCallbackInfo& info) {
    if (e.button != BTN_LEFT || (!captured && !visible())) return;
    const bool down = e.state == WL_POINTER_BUTTON_STATE_PRESSED;
    const auto p = g_pInputManager->getMouseCoordsInternal();
    if (!down && captured) {
        info.cancelled = true;
        const int action = visible() && inside(p, box()) && hitButton(p-box().pos()) == pressed ? pressed : 0;
        cancelGrab(); damageEntire();
        if (action == 2) window->sendClose();
        if (action == 1 && !cfg.minimize->value().empty()) {
            Desktop::focusState()->fullWindowFocus(window.lock(), Desktop::FOCUS_REASON_CLICK);
            HyprlandAPI::invokeHyprctlCommand("dispatch", cfg.minimize->value());
        }
        return;
    }
    if (!down || info.cancelled || !acceptsInput(p)) return;
    auto w = window.lock();
    Desktop::focusState()->fullWindowFocus(w, Desktop::FOCUS_REASON_CLICK);
    if (w->isFloating()) Desktop::windowState()->raise(w);
    info.cancelled = true;
    captured = true;
    start = p;
    if (bottom) {
        auto x = p.x-box().x;
        int cw = cornerWidth(box().w);
        auto edge = x <= cw+1 ? Layout::CORNER_BOTTOMLEFT : x >= box().w-cw-2 ? Layout::CORNER_BOTTOMRIGHT : Layout::CORNER_BOTTOM;
        g_layoutManager->beginDragTarget(w->layoutTarget(), MBIND_RESIZE, edge);
        dragging = true;
    } else {
        pressed = hitButton(p-box().pos());
        if (pressed) { damageEntire(); return; }
        const auto now = Time::steadyNow();
        if (cfg.doubleClickMs->value() > 0 && std::chrono::duration_cast<std::chrono::milliseconds>(now-lastClick).count() <= cfg.doubleClickMs->value() &&
            (p-lastClickPos).size() < 5) {
            lastClick = {};
            if (!cfg.doubleClick->value().empty()) HyprlandAPI::invokeHyprctlCommand("dispatch", cfg.doubleClick->value());
        } else {
            lastClick = now; lastClickPos = p;
            pending = true;
        }
    }
}
void Deco::motion(Vector2D p) {
    if (!captured) return; // no damage/texture work for idle or fullscreen windows
    if (!visible()) { cancelGrab(); return; }
    if (pending && (p-start).size() >= 3) {
        pending = false; dragging = true; lastClick = {};
        g_layoutManager->beginDragTarget(window->layoutTarget(), MBIND_MOVE);
    }
}
