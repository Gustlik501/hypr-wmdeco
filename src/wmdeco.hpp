// SPDX-License-Identifier: GPL-2.0-or-later
// Decoration integration derived from hyprbars v0.57.0 (BSD-3-Clause).
#pragma once
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/config/values/ConfigValues.hpp>
#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>
#include <hyprland/src/render/gl/GLTexture.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/helpers/signal/Signal.hpp>
#include <array>
#include <deque>
#include <cairo.h>

namespace WM {
using namespace Config::Values;
struct TextureConfig {
    SP<CStringValue> type;
    SP<CColorValue> color1, color2;
};
struct ConfigState {
    SP<CBoolValue> enabled;
    SP<CStringValue> font, justify, excludeTags, minimize, doubleClick;
    SP<CIntValue> fontSize, fontWeight, height, buttonSize, buttonPadding, titlePadding, resizeHeight, cornerWidth, doubleClickMs;
    std::array<TextureConfig, 3> title, button;
    TextureConfig resize;
    std::array<SP<CColorValue>, 3> text;
    int autoHeight = 22;
};
class Deco;
inline HANDLE handle = nullptr;
inline ConfigState cfg;
inline std::vector<WP<Deco>> decos;
inline std::deque<std::string> configNames;
inline std::vector<CHyprSignalListener> listeners;
inline uint64_t epoch = 1, paintCount = 0, renderCount = 0;
inline int noBarRule = -1;
void refresh();
int fontHeight();
int titleHeight();
int buttonSize();

struct Texture {
    std::string type;
    uint32_t a, b;
};
struct PaintSpec {
    bool bottom = false;
    int width, height, button, padding, titlePadding, corner, pressed;
    double scale;
    Texture texture, buttonTexture;
    uint32_t textColor;
    std::string title, font, justify;
    int fontSize, weight;
};
Texture texture(const TextureConfig&);
cairo_surface_t* paint(const PaintSpec&);

class Deco : public IHyprWindowDecoration {
  public:
    Deco(PHLWINDOW, bool bottom);
    ~Deco() override;
    SDecorationPositioningInfo getPositioningInfo() override;
    void onPositioningReply(const SDecorationPositioningReply&) override;
    void draw(Render::CRenderContext&, PHLMONITOR, const float&, const Render::SWindowRenderPresentation&) override;
    eDecorationType getDecorationType() override { return DECORATION_CUSTOM; }
    eDecorationLayer getDecorationLayer() override { return DECORATION_LAYER_UNDER; }
    uint64_t getDecorationFlags() override { return DECORATION_ALLOWS_MOUSE_INPUT | DECORATION_PART_OF_MAIN_WINDOW; }
    std::string getDisplayName() override { return bottom ? "WMResizebar" : "WMTitlebar"; }
    void updateWindow(PHLWINDOW) override;
    void damageEntire() override;
    PHLWINDOW owner() const { return window.lock(); }
    CBox box();
    void sync();
    void render(Render::CRenderContext&, PHLMONITOR, float);
    bool visible() const;
  private:
    PHLWINDOWREF window;
    bool bottom, hidden = false;
    CBox assigned;
    SP<Render::ITexture> cached;
    int lastWidth = -1, lastHeight = -1, lastState = -1, lastPressed = -1;
    double lastScale = 0;
    uint64_t lastEpoch = 0;
    std::string lastTitle;
    CHyprSignalListener mouseButton, mouseMove;
    bool captured = false, pending = false, dragging = false;
    int pressed = 0;
    Vector2D start;
    Time::steady_tp lastClick{};
    Vector2D lastClickPos;
    void button(IPointer::SButtonEvent, Event::SCallbackInfo&);
    void motion(Vector2D);
    void cancelGrab();
    int hitButton(Vector2D) const;
    bool acceptsInput(Vector2D);
};
}
