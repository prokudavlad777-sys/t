#include "Tab.h"
#include "../Core/App.h"
#include <thread>

using namespace Gdiplus;

void Tab::Begin() { widgets_.clear(); }

void Tab::Btn(Graphics& g, const RectF& r, const std::wstring& text, int id, bool primary, bool enabled) {
    widgets_.push_back({r, id, enabled, false});
    bool hot = enabled && Hover(r);
    bool down = hot && pressed_ == id;
    RectF rr = r;
    if (down) rr.Y += 2;                                    // pressed: the clay button sinks a little
    float rad = r.Height / 2;
    ARGB fg;
    if (!enabled)     { ui::Clay(g, rr, rad, 0xFFEEE9F8, 0.3f); fg = ui::col::muted; }
    else if (primary) { ui::Clay(g, rr, rad, hot ? ui::Shade(ui::col::purple, 14) : ui::col::purple, down ? 0.3f : 0.6f); fg = 0xFFFFFFFF; }
    else              { ui::Clay(g, rr, rad, hot ? ui::Shade(ui::col::blue, 12) : ui::col::blue, down ? 0.3f : 0.6f); fg = ui::col::text; }
    ui::Text(g, text, RectF(rr.X + 10, rr.Y, rr.Width - 20, rr.Height), 14, fg, true, StringAlignmentCenter, StringAlignmentCenter);
}

void Tab::Area(const RectF& r, int id) { widgets_.push_back({r, id, true, false}); }

void Tab::Slide(Graphics& g, const RectF& r, int id, float v) {
    widgets_.push_back({r, id, true, true});
    v = v < 0 ? 0 : (v > 1 ? 1 : v);
    float cy = r.Y + r.Height / 2;
    ui::Inset(g, RectF(r.X, cy - 6, r.Width, 12), 6, ui::col::card2);
    float kx = r.X + 8 + (r.Width - 16) * v;
    ui::Fill(g, RectF(r.X + 3, cy - 3, max(6.f, kx - r.X - 3), 6), 3, ui::col::purple);
    bool hot = Hover(r) || drag_ == id;
    ui::Clay(g, RectF(kx - 13, cy - 13, 26, 26), 13, hot ? 0xFFFFFFFF : 0xFFFBF9FF, 0.35f);
    ui::Fill(g, RectF(kx - 5, cy - 5, 10, 10), 5, ui::col::purple);
}

void Tab::Check(Graphics& g, const RectF& r, const std::wstring& text, int id, bool checked) {
    widgets_.push_back({r, id, true, false});
    RectF box(r.X, r.Y + (r.Height - 28) / 2, 28, 28);
    if (checked) {
        ui::Clay(g, box, 10, ui::col::purple, 0.3f);
        Pen pen(Color(0xFFFFFFFF), 3.f);
        pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
        g.DrawLine(&pen, box.X + 7, box.Y + 14.5f, box.X + 12, box.Y + 19.5f);
        g.DrawLine(&pen, box.X + 12, box.Y + 19.5f, box.X + 21, box.Y + 8.5f);
    } else {
        ui::Inset(g, box, 10, Hover(r) ? 0xFFE6DCFF : ui::col::card2);
    }
    ui::Text(g, text, RectF(r.X + 40, r.Y, r.Width - 40, r.Height), 14, ui::col::text, false,
             StringAlignmentNear, StringAlignmentCenter);
}

void Tab::TextBox(Graphics& g, const RectF& r, const std::wstring& placeholder, const std::wstring& text, int id,
                  bool focused) {
    widgets_.push_back({r, id, true, false});
    ui::Inset(g, r, r.Height / 2, ui::col::card2);
    if (focused) ui::Stroke(g, r, r.Height / 2, ui::col::purple, 2.f);
    RectF tr(r.X + 20, r.Y, r.Width - 40, r.Height);
    if (text.empty()) ui::Text(g, placeholder, tr, 15, ui::col::muted, false, StringAlignmentNear, StringAlignmentCenter);
    else ui::Text(g, text, tr, 15, ui::col::text, false, StringAlignmentNear, StringAlignmentCenter);
    if (focused && (GetTickCount64() / 500) % 2 == 0) {
        float x = tr.X + ui::TextWidth(g, text, 15) + 1;
        ui::Fill(g, RectF(x, r.Y + 11, 2, r.Height - 22), 1, ui::col::purple);
    }
}

void Tab::MouseDown(PointF p) {
    mouse_ = p;
    pressed_ = drag_ = -1;
    for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it) {
        if (!it->enabled || !it->r.Contains(p)) continue;
        if (it->slider) { drag_ = it->id; MouseMove(p); }
        else pressed_ = it->id;
        return;
    }
}

void Tab::MouseMove(PointF p) {
    mouse_ = p;
    if (drag_ < 0) return;
    for (auto& w : widgets_) {
        if (w.slider && w.id == drag_) {
            float v = (p.X - w.r.X) / w.r.Width;
            OnSlider(drag_, v < 0 ? 0 : (v > 1 ? 1 : v));
            return;
        }
    }
}

void Tab::MouseUp(PointF p) {
    mouse_ = p;
    if (drag_ >= 0) { drag_ = -1; return; }
    int id = pressed_;
    pressed_ = -1;
    if (id < 0) return;
    for (auto& w : widgets_)
        if (!w.slider && w.id == id && w.enabled && w.r.Contains(p)) { OnButton(id); return; }
}

bool Tab::RunAsync(std::function<void()> work) {
    bool expected = false;
    if (!busy_.compare_exchange_strong(expected, true)) return false;
    std::thread([this, work]() {
        work();
        busy_ = false;
        if (app::requestRepaint) app::requestRepaint();
    }).detach();
    return true;
}
