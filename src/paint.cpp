// SPDX-License-Identifier: GPL-2.0-or-later
// WM geometry/relief and 10x10 masks based on Window Maker 0.96.0.
// Original Window Maker / wraster algorithms: Copyright (c) 1997-2003 Alfredo K. Kojima.
// Wraster's LGPL-2.0-or-later algorithms used under GPL per LGPL section 3.
// See SOURCES.md and LICENSE for the original authors and source locations.
#include "wmdeco.hpp"
#include <pango/pangocairo.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace WM {
namespace {
struct RGB {
    int r, g, b;
    explicit RGB(uint32_t c) : r((c >> 16) & 255), g((c >> 8) & 255), b(c & 255) {}
    RGB(int r, int g, int b) : r(r), g(g), b(b) {}
    uint32_t argb() const { return 0xff000000u | (r << 16) | (g << 8) | b; }
    RGB add(int n) const { return {std::clamp(r+n, 0, 255), std::clamp(g+n, 0, 255), std::clamp(b+n, 0, 255)}; }
    RGB mul(double n) const { return {std::min(255, int(r*n)), std::min(255, int(g*n)), std::min(255, int(b*n))}; }
};
RGB sample(const Texture& t, int x, int y, int w, int h) {
    RGB a(t.a), b(t.b);
    int index = 0, length = 1;
    if (t.type == "hgradient") { index = x; length = w; }
    if (t.type == "vgradient") { index = y; length = h; }
    if (t.type == "dgradient") {
        if (w == 1) { index = y; length = h; }
        else if (h == 1) { index = x; length = w; }
        else { index = x + int(float(y)*float(w-1)/float(h-1)); length = 2*w-1; }
    }
    // wraster uses signed 16.16 channel increments divided by length, not length-1.
    auto channel = [=](int from, int to) { return int((int64_t(from)*65536 + (int64_t(to-from)*65536/std::max(1,length))*index) >> 16); };
    return {channel(a.r,b.r), channel(a.g,b.g), channel(a.b,b.b)};
}
std::pair<RGB, RGB> solidEdges(RGB c) {
    if (!c.r && !c.g && !c.b) return {RGB(182,182,182), RGB(97,97,97)};
    // wTextureMakeSolid: keep HSV hue/saturation, value * 1.6 or / 2.
    const int v = std::max({c.r, c.g, c.b});
    return {c.mul(double(std::min(255, v*16/10))/v), c.mul(double(v/2)/v)};
}
RGB relief(RGB c, const Texture& t, int x, int y, int w, int h, bool pushed) {
    if (w < 3 || h < 3) return c;
    if (pushed) {
        if (y == 0 || x == 0) c = c.add(-40);
        if (y == h-1 || x == w-1) c = c.add(80);
        return c;
    }
    if (t.type == "solid") {
        auto [light, dim] = solidEdges(RGB(t.a));
        if (y == h-2 && x >= 1 && x <= w-2) c = dim;
        if (x == w-2 && y >= 1 && y <= h-2) c = dim;
        if (y == h-1 || x == w-1) c = RGB(0,0,0);
        if ((y == 0 && x <= w-2) || (x == 0 && y <= h-2)) c = light;
    } else {
        // wraster RBEV_RAISED2: additive light, subtractive dim, black outer edge.
        if (y == 0 || (x == 0 && y >= 1)) c = c.add(80);
        if (y == h-2 && x <= w-3) c = c.add(-40);
        if (y == h-1) c = RGB(0,0,0);
        if (x == w-2 && y <= h-2) c = c.add(-40);
        if (x == w-1 && y <= h-2) c = RGB(0,0,0);
    }
    return c;
}
PangoFontDescription* fontDescription() {
    auto f = pango_font_description_new();
    pango_font_description_set_family(f, cfg.font->value().c_str());
    pango_font_description_set_absolute_size(f, cfg.fontSize->value()*PANGO_SCALE);
    pango_font_description_set_weight(f, PangoWeight(cfg.fontWeight->value()));
    return f;
}
int textWidth(PangoLayout* layout, const std::string& s) {
    pango_layout_set_text(layout, s.c_str(), s.size());
    int w; pango_layout_get_pixel_size(layout, &w, nullptr);
    return w;
}
// WM's ShrinkString keeps the first word (or prefix before ':') and the tail,
// not a conventional trailing ellipsis. Work at UTF-8 boundaries, unlike Xlib.
std::string shrink(PangoLayout* layout, const std::string& text, int width) {
    if (textWidth(layout, text) <= width) return text;
    std::string prefix, tail = text;
    auto p = text.find(' ');
    if (p == std::string::npos) p = text.find(':');
    if (p != std::string::npos && textWidth(layout, text.substr(0,p)) <= width) {
        prefix = text.substr(0,p);
        tail = text.substr(p+1);
    }
    prefix += "...";
    std::vector<size_t> offsets;
    for (size_t i = 0; i < tail.size(); ++i)
        if ((static_cast<unsigned char>(tail[i]) & 0xc0) != 0x80) offsets.push_back(i);
    offsets.push_back(tail.size());
    size_t lo = 0, hi = offsets.size()-1;
    while (lo < hi) {
        size_t mid = (lo+hi)/2;
        if (textWidth(layout, prefix + tail.substr(offsets[mid])) <= width) hi = mid;
        else lo = mid+1;
    }
    return prefix + tail.substr(offsets[lo]);
}
// Window Maker src/def_pixmaps.h PRED_CLOSE_XPM/PRED_ICONIFY_XPM masks.
constexpr const char* CLOSE[] = {" .XXXXXX. ",". .XXXX. .","X. .XX. .X","XX. .. .XX","XXX.  .XXX","XXX.  .XXX","XX. .. .XX","X. .XX. .X",". .XXXX. ."," .XXXXXX. "};
constexpr const char* MINI[] = {"..........","..........","..........",".########.",".########.",".########.",".########.",".########.",".########.",".........."};
}
int fontHeight() {
    auto fm = pango_cairo_font_map_get_default();
    auto context = pango_font_map_create_context(fm);
    auto f = fontDescription();
    auto metrics = pango_context_get_metrics(context, f, pango_language_get_default());
    int height = PANGO_PIXELS_CEIL(pango_font_metrics_get_ascent(metrics) + pango_font_metrics_get_descent(metrics));
    pango_font_metrics_unref(metrics); pango_font_description_free(f); g_object_unref(context);
    return height;
}
cairo_surface_t* paint(const PaintSpec& s) {
    const int pw = std::max(1, int(std::round(s.width*s.scale))), ph = std::max(1, int(std::round(s.height*s.scale)));
    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
    auto data = cairo_image_surface_get_data(surface);
    const int stride = cairo_image_surface_get_stride(surface);
    const int bs = s.button;
    const bool left = s.width >= 4*s.height && bs > 0;
    const bool right = s.width >= 2*s.height && bs > 0;
    const int by = (s.height-bs)/2;
    const int lx = s.padding, rx = s.width-s.padding-bs;
    const int titleStart = left ? lx+bs : 0, titleEnd = right ? rx : s.width;
    for (int py=0; py<ph; ++py) for (int px=0; px<pw; ++px) {
        int x = std::min(s.width-1, int(px/s.scale)), y = std::min(s.height-1, int(py/s.scale));
        RGB c = sample(s.texture, x, y, s.width, s.height);
        if (s.bottom) {
            bool dim = y == 0 || (y >= 2 && (x == s.corner || x == s.width-s.corner-2));
            bool light = y == 1 || (y >= 2 && (x == s.corner+1 || x == s.width-s.corner-1));
            if (s.texture.type == "solid") {
                auto edges = solidEdges(RGB(s.texture.a));
                if (dim) c = edges.second;
                if (light) c = edges.first;
            } else {
                if (dim) c = c.add(-40);
                if (light) c = c.add(80);
            }
        } else {
            int btn = 0, bx = 0;
            if (left && x >= lx && x < lx+bs && y >= by && y < by+bs) { btn = 1; bx = lx; }
            if (right && x >= rx && x < rx+bs && y >= by && y < by+bs) { btn = 2; bx = rx; }
            if (btn) {
                const bool inherit = s.buttonTexture.type == "inherit";
                const Texture& t = inherit ? s.texture : s.buttonTexture;
                if (!inherit) c = sample(t, x-bx, y-by, bs, bs);
                c = relief(c, t, x-bx, y-by, bs, bs, btn == s.pressed);
                const int offset = btn == s.pressed ? 1 : 0;
                int ix = x-bx-(bs-10)/2-offset, iy = y-by-(bs-10)/2-offset;
                if (ix >= 0 && ix < 10 && iy >= 0 && iy < 10 && bs >= 12) {
                    char v = (btn == 1 ? MINI : CLOSE)[iy][ix];
                    if ((btn == 1 && v != '#') || (btn == 2 && v != 'X')) c = RGB(s.textColor);
                }
            } else if (x >= titleStart && x < titleEnd) c = relief(c, s.texture, x-titleStart, y, titleEnd-titleStart, s.height, false);
        }
        reinterpret_cast<uint32_t*>(data+py*stride)[px] = c.argb();
    }
    cairo_surface_mark_dirty(surface);
    if (!s.bottom) {
        auto cr = cairo_create(surface);
        cairo_scale(cr, s.scale, s.scale);
        cairo_rectangle(cr, titleStart+s.titlePadding, 1, std::max(0,titleEnd-titleStart-2*s.titlePadding), std::max(0,s.height-2));
        cairo_clip(cr);
        auto layout = pango_cairo_create_layout(cr);
        pango_layout_set_single_paragraph_mode(layout, true);
        auto f = pango_font_description_new();
        pango_font_description_set_family(f, s.font.c_str());
        pango_font_description_set_absolute_size(f, s.fontSize*PANGO_SCALE);
        pango_font_description_set_weight(f, PangoWeight(s.weight));
        pango_layout_set_font_description(layout, f);
        auto title = shrink(layout, s.title, std::max(0,titleEnd-titleStart-2*s.titlePadding));
        pango_layout_set_text(layout, title.c_str(), title.size());
        int tw, th; pango_layout_get_pixel_size(layout, &tw, &th);
        int tx = (titleStart+titleEnd-tw)/2;
        if (s.justify == "left") tx = titleStart+s.titlePadding;
        if (s.justify == "right") tx = titleEnd-s.titlePadding-tw;
        RGB fg(s.textColor);
        cairo_set_source_rgb(cr, fg.r/255.0, fg.g/255.0, fg.b/255.0);
        cairo_move_to(cr, tx, (s.height-th)/2);
        pango_cairo_show_layout(cr, layout);
        pango_font_description_free(f); g_object_unref(layout); cairo_destroy(cr);
    }
    cairo_surface_flush(surface);
    return surface;
}
}
