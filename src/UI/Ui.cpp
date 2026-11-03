#include "Ui.h"
#include "../Core/Util.h"
#include <map>
#include <memory>

namespace ui {

ARGB WithAlpha(ARGB c, BYTE a) { return (c & 0x00FFFFFF) | ((ARGB)a << 24); }

ARGB Shade(ARGB c, int d) {
    auto ch = [&](int v) { return (ARGB)(v + d < 0 ? 0 : (v + d > 255 ? 255 : v + d)); };
    return (c & 0xFF000000) | (ch((c >> 16) & 0xFF) << 16) | (ch((c >> 8) & 0xFF) << 8) | ch(c & 0xFF);
}

static std::unique_ptr<FontFamily> g_family;
static std::map<int, std::unique_ptr<Font>> g_fonts;
static std::map<std::wstring, std::unique_ptr<Bitmap>> g_images;
static std::map<std::wstring, std::unique_ptr<Bitmap>> g_avatars;
static std::unique_ptr<Bitmap> g_logo;
static bool g_logoTried = false;

static Font* GetFont(float px, bool bold) {
    if (!g_family) {
        g_family.reset(new FontFamily(L"Segoe UI"));
        if (!g_family->IsAvailable()) g_family.reset(FontFamily::GenericSansSerif()->Clone());
    }
    int key = (int)(px * 10) + (bold ? 100000 : 0);
    auto it = g_fonts.find(key);
    if (it != g_fonts.end()) return it->second.get();
    Font* f = new Font(g_family.get(), px, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    g_fonts[key].reset(f);
    return f;
}

void Text(Graphics& g, const std::wstring& s, const RectF& r, float px, ARGB color, bool bold,
          StringAlignment h, StringAlignment v, bool wrap) {
    if (s.empty()) return;
    StringFormat fmt;
    fmt.SetAlignment(h);
    fmt.SetLineAlignment(v);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    if (!wrap) fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    SolidBrush br{Color(color)};
    g.DrawString(s.c_str(), -1, GetFont(px, bold), r, &fmt, &br);
}

float TextWidth(Graphics& g, const std::wstring& s, float px, bool bold) {
    if (s.empty()) return 0;
    RectF box;
    g.MeasureString(s.c_str(), -1, GetFont(px, bold), PointF(0, 0), StringFormat::GenericTypographic(), &box);
    return box.Width;
}

static void RoundPath(GraphicsPath& p, const RectF& r, float rad) {
    float d = min(rad * 2, min(r.Width, r.Height));
    if (d <= 0.5f) { p.AddRectangle(r); return; }
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    p.CloseFigure();
}

void Fill(Graphics& g, const RectF& r, float radius, ARGB fill) {
    GraphicsPath p;
    RoundPath(p, r, radius);
    SolidBrush br{Color(fill)};
    g.FillPath(&br, &p);
}

void Stroke(Graphics& g, const RectF& r, float radius, ARGB color, float width) {
    GraphicsPath p;
    RoundPath(p, r, radius);
    Pen pen(Color(color), width);
    g.DrawPath(&pen, &p);
}

// Soft shadow = a few translucent, growing copies of the shape.
static void SoftShadow(Graphics& g, const RectF& r, float rad, float dx, float dy, int layers, float spread,
                       BYTE alpha, ARGB rgb) {
    for (int i = 0; i < layers; ++i) {
        float e = spread * i;
        Fill(g, RectF(r.X + dx - e, r.Y + dy - e, r.Width + 2 * e, r.Height + 2 * e), rad + e, WithAlpha(rgb, alpha));
    }
}

void Clay(Graphics& g, const RectF& r, float rad, ARGB fill, float d) {
    SoftShadow(g, r, rad, 7 * d, 9 * d, 5, 2.0f * d + 0.4f, 12, 0xFF8A6FD8);      // outer: dark, bottom-right
    SoftShadow(g, r, rad, -6 * d, -6 * d, 4, 1.8f * d + 0.3f, 80, 0xFFFFFFFF);    // outer: light, top-left

    GraphicsPath p;
    RoundPath(p, r, rad);
    LinearGradientBrush body(PointF(r.X, r.Y), PointF(r.X + r.Width, r.Y + r.Height), Color(Shade(fill, 10)),
                             Color(Shade(fill, -8)));
    g.FillPath(&body, &p);

    float o = 2.5f * d + 1.f, w = 4.f * d + 1.f;
    GraphicsState st = g.Save();                                                   // inner: highlight, top-left
    g.SetClip(&p);
    g.TranslateTransform(o, o);
    Pen hi(Color(WithAlpha(0xFFFFFFFF, 170)), w);
    g.DrawPath(&hi, &p);
    g.Restore(st);

    st = g.Save();                                                                 // inner: shade, bottom-right
    g.SetClip(&p);
    g.TranslateTransform(-o, -o);
    Pen lo(Color(WithAlpha(Shade(fill, -50), 80)), w);
    g.DrawPath(&lo, &p);
    g.Restore(st);
}

void Inset(Graphics& g, const RectF& r, float rad, ARGB fill) {
    GraphicsPath p;
    RoundPath(p, r, rad);
    SolidBrush body{Color(fill)};
    g.FillPath(&body, &p);

    GraphicsState st = g.Save();
    g.SetClip(&p);
    g.TranslateTransform(3, 3);
    Pen dark(Color(WithAlpha(Shade(fill, -60), 110)), 6.f);
    g.DrawPath(&dark, &p);
    g.Restore(st);

    st = g.Save();
    g.SetClip(&p);
    g.TranslateTransform(-2.5f, -2.5f);
    Pen light(Color(WithAlpha(0xFFFFFFFF, 230)), 5.f);
    g.DrawPath(&light, &p);
    g.Restore(st);
}

void Card(Graphics& g, const RectF& r, const std::wstring& title) {
    Clay(g, r, 28, col::surface, 1.f);
    if (!title.empty()) Text(g, title, RectF(r.X + 22, r.Y + 14, r.Width - 44, 26), 16, col::text, true);
}

void Bar(Graphics& g, const RectF& r, double frac, ARGB color) {
    frac = frac < 0 ? 0 : (frac > 1 ? 1 : frac);
    Inset(g, r, r.Height / 2, col::card2);
    if (frac > 0.001) {
        float w = max(r.Height - 4, (float)((r.Width - 4) * frac));
        RectF f(r.X + 2, r.Y + 2, w, r.Height - 4);
        Fill(g, f, f.Height / 2, color);
        Fill(g, RectF(f.X + 5, f.Y + 2, max(2.f, f.Width - 10), f.Height * 0.34f), f.Height / 4, WithAlpha(0xFFFFFFFF, 110));
    }
}

ARGB TabColor(const std::wstring& n) {
    if (n == L"home" || n == L"settings") return col::lavender;
    if (n == L"acceleration" || n == L"crosshairs") return col::pink;
    if (n == L"cleaning" || n == L"colors") return col::blue;
    return col::mint;
}

void TabIcon(Graphics& g, const std::wstring& name, const RectF& r, ARGB color) {
    auto P = [&](float x, float y) { return PointF(r.X + x * r.Width, r.Y + y * r.Height); };
    float w = max(1.8f, r.Width / 9.f);
    Pen pen(Color(color), w);
    pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    pen.SetLineJoin(LineJoinRound);
    SolidBrush br{Color(color)};
    auto circle = [&](float x, float y, float s, bool fill) {
        RectF e(r.X + x * r.Width, r.Y + y * r.Height, s * r.Width, s * r.Height);
        if (fill) g.FillEllipse(&br, e); else g.DrawEllipse(&pen, e);
    };
    if (name == L"home") {
        PointF roof[3] = {P(0.08f, 0.50f), P(0.50f, 0.12f), P(0.92f, 0.50f)};
        g.DrawLines(&pen, roof, 3);
        PointF body[4] = {P(0.20f, 0.44f), P(0.20f, 0.88f), P(0.80f, 0.88f), P(0.80f, 0.44f)};
        g.DrawLines(&pen, body, 4);
        g.DrawLine(&pen, P(0.50f, 0.88f), P(0.50f, 0.62f));
    } else if (name == L"acceleration") {
        PointF bolt[6] = {P(0.58f, 0.06f), P(0.20f, 0.56f), P(0.47f, 0.56f), P(0.40f, 0.94f), P(0.80f, 0.40f), P(0.53f, 0.40f)};
        g.FillPolygon(&br, bolt, 6);
    } else if (name == L"cleaning") {
        g.DrawLine(&pen, P(0.78f, 0.08f), P(0.52f, 0.46f));
        PointF bristles[4] = {P(0.34f, 0.50f), P(0.66f, 0.50f), P(0.78f, 0.92f), P(0.18f, 0.92f)};
        g.FillPolygon(&br, bristles, 4);
        circle(0.12f, 0.12f, 0.12f, true);
    } else if (name == L"programs") {
        RectF win(r.X + 0.08f * r.Width, r.Y + 0.14f * r.Height, 0.84f * r.Width, 0.72f * r.Height);
        GraphicsPath wp;
        RoundPath(wp, win, r.Width * 0.14f);
        g.DrawPath(&pen, &wp);
        g.DrawLine(&pen, P(0.08f, 0.38f), P(0.92f, 0.38f));
        g.DrawLine(&pen, P(0.30f, 0.76f), P(0.30f, 0.60f));
        g.DrawLine(&pen, P(0.50f, 0.76f), P(0.50f, 0.52f));
        g.DrawLine(&pen, P(0.70f, 0.76f), P(0.70f, 0.64f));
    } else if (name == L"crosshairs") {
        circle(0.22f, 0.22f, 0.56f, false);
        circle(0.43f, 0.43f, 0.14f, true);
        g.DrawLine(&pen, P(0.50f, 0.04f), P(0.50f, 0.24f));
        g.DrawLine(&pen, P(0.50f, 0.76f), P(0.50f, 0.96f));
        g.DrawLine(&pen, P(0.04f, 0.50f), P(0.24f, 0.50f));
        g.DrawLine(&pen, P(0.76f, 0.50f), P(0.96f, 0.50f));
    } else if (name == L"colors") {
        circle(0.30f, 0.06f, 0.44f, false);
        circle(0.08f, 0.46f, 0.44f, false);
        circle(0.52f, 0.46f, 0.44f, false);
    } else if (name == L"drivers") {
        RectF chip(r.X + 0.28f * r.Width, r.Y + 0.28f * r.Height, 0.44f * r.Width, 0.44f * r.Height);
        GraphicsPath cp;
        RoundPath(cp, chip, r.Width * 0.08f);
        g.DrawPath(&pen, &cp);
        for (float t : {0.38f, 0.62f}) {
            g.DrawLine(&pen, P(t, 0.08f), P(t, 0.28f));
            g.DrawLine(&pen, P(t, 0.72f), P(t, 0.92f));
            g.DrawLine(&pen, P(0.08f, t), P(0.28f, t));
            g.DrawLine(&pen, P(0.72f, t), P(0.92f, t));
        }
    } else if (name == L"settings") {
        g.DrawLine(&pen, P(0.10f, 0.24f), P(0.90f, 0.24f));
        g.DrawLine(&pen, P(0.10f, 0.50f), P(0.90f, 0.50f));
        g.DrawLine(&pen, P(0.10f, 0.76f), P(0.90f, 0.76f));
        circle(0.52f, 0.14f, 0.20f, true);
        circle(0.20f, 0.40f, 0.20f, true);
        circle(0.62f, 0.66f, 0.20f, true);
    }
}

void DrawLogo(Graphics& g, const RectF& r) {
    if (!g_logoTried) {
        g_logoTried = true;
        std::unique_ptr<Bitmap> src(Bitmap::FromFile(util::Path(L"assets\\icons\\logo.png").c_str()));
        if (src && src->GetLastStatus() == Ok) {
            const int crop = 150;
            int S = (int)src->GetWidth() - 2 * crop;
            if (S > 16 && (int)src->GetHeight() >= S + 2 * crop) {
                std::unique_ptr<Bitmap> out(new Bitmap(S, S, PixelFormat32bppARGB));
                BitmapData sd{}, dd{};
                Rect sr(crop, crop, S, S), dr(0, 0, S, S);
                if (src->LockBits(&sr, ImageLockModeRead, PixelFormat32bppARGB, &sd) == Ok) {
                    if (out->LockBits(&dr, ImageLockModeWrite, PixelFormat32bppARGB, &dd) == Ok) {
                        for (int y = 0; y < S; ++y) {
                            const UINT32* s = (const UINT32*)((const BYTE*)sd.Scan0 + (size_t)y * sd.Stride);
                            UINT32* d = (UINT32*)((BYTE*)dd.Scan0 + (size_t)y * dd.Stride);
                            for (int x = 0; x < S; ++x) {
                                UINT32 a = (s[x] >> 8) & 0xFF;       // green channel of the lime "B" = coverage
                                d[x] = a < 14 ? 0 : ((a << 24) | (col::purple & 0x00FFFFFF));
                            }
                        }
                        out->UnlockBits(&dd);
                    }
                    src->UnlockBits(&sd);
                    g_logo = std::move(out);
                }
            }
        }
    }
    if (g_logo) {
        InterpolationMode old = g.GetInterpolationMode();
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.DrawImage(g_logo.get(), r);
        g.SetInterpolationMode(old);
    } else {
        Text(g, L"B", r, r.Height * 0.8f, col::purple, true, StringAlignmentCenter, StringAlignmentCenter);
    }
}

void Avatar(Graphics& g, const RectF& r, Bitmap* img, ARGB color, const std::wstring& initials) {
    Clay(g, r, r.Width / 2, col::surface, 0.7f);
    RectF in(r.X + 5, r.Y + 5, r.Width - 10, r.Height - 10);
    if (img) {
        GraphicsPath p;
        p.AddEllipse(in);
        GraphicsState st = g.Save();
        g.SetClip(&p);
        g.DrawImage(img, in);
        g.Restore(st);
    } else {
        Fill(g, in, in.Width / 2, color);
        Fill(g, RectF(in.X + in.Width * 0.18f, in.Y + 5, in.Width * 0.64f, in.Height * 0.22f), in.Height * 0.11f,
             WithAlpha(0xFFFFFFFF, 90));
        Text(g, initials, in, in.Width * 0.36f, col::text, true, StringAlignmentCenter, StringAlignmentCenter);
    }
}

Bitmap* AvatarImage(const std::wstring& rel) {
    if (rel.empty()) return nullptr;
    auto it = g_avatars.find(rel);
    if (it != g_avatars.end()) return it->second.get();
    std::unique_ptr<Bitmap> out;
    {
        std::unique_ptr<Bitmap> src(Bitmap::FromFile(util::Path(rel).c_str()));
        if (src && src->GetLastStatus() == Ok && src->GetWidth() > 0 && src->GetHeight() > 0) {
            const int N = 256;
            out.reset(new Bitmap(N, N, PixelFormat32bppARGB));
            Graphics gg(out.get());
            gg.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            float w = (float)src->GetWidth(), h = (float)src->GetHeight();
            float s = max((float)N / w, (float)N / h);
            gg.DrawImage(src.get(), RectF((N - w * s) / 2, (N - h * s) / 2, w * s, h * s));   // "cover" crop
        }
    }                                                    // the original file is released here
    Bitmap* raw = out.get();
    g_avatars[rel] = std::move(out);
    return raw;
}

Bitmap* Image(const std::wstring& rel) {
    auto it = g_images.find(rel);
    if (it != g_images.end()) return it->second.get();
    std::unique_ptr<Bitmap> bmp(Bitmap::FromFile(util::Path(rel).c_str()));
    if (bmp && bmp->GetLastStatus() != Ok) bmp.reset();
    Bitmap* raw = bmp.get();
    g_images[rel] = std::move(bmp);
    return raw;
}

void Forget(const std::wstring& rel) {
    g_images.erase(rel);
    g_avatars.erase(rel);
}

void Shutdown() {
    g_images.clear();
    g_avatars.clear();
    g_logo.reset();
    g_fonts.clear();
    g_family.reset();
}
}
