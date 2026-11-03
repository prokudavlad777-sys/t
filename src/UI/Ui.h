#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
using std::min;
using std::max;
#include <gdiplus.h>
#include <string>

// Claymorphism: big rounded shapes, soft volumetric surfaces, pastel colours, double shadows.
namespace ui {
using namespace Gdiplus;

namespace col {
constexpr ARGB bg       = 0xFFF4EFFF;   // Background
constexpr ARGB surface  = 0xFFFFFFFF;   // Surface
constexpr ARGB lavender = 0xFFC9B6FF;   // Lavender
constexpr ARGB purple   = 0xFF9B7BFF;   // Purple
constexpr ARGB pink     = 0xFFFFB8D2;   // Pink
constexpr ARGB blue     = 0xFFA8D8FF;   // Blue
constexpr ARGB mint     = 0xFFA9EBCB;   // Mint
constexpr ARGB text     = 0xFF38304D;   // Text
// derived
constexpr ARGB side     = surface;
constexpr ARGB card     = surface;
constexpr ARGB card2    = 0xFFEFE8FF;   // sunken fields / tracks
constexpr ARGB line     = 0xFFE2D8FA;
constexpr ARGB accent   = purple;
constexpr ARGB accentD  = 0xFF7F5FE6;
constexpr ARGB muted    = 0xFF8C84A6;
constexpr ARGB danger   = 0xFFFF7BA8;
constexpr ARGB warn     = 0xFFFFC46B;
constexpr ARGB good     = 0xFF5FCFA0;
}

ARGB WithAlpha(ARGB c, BYTE a);
ARGB Shade(ARGB c, int delta);                     // lighten (+) / darken (-)

void Text(Graphics& g, const std::wstring& s, const RectF& r, float px, ARGB color, bool bold = false,
          StringAlignment h = StringAlignmentNear, StringAlignment v = StringAlignmentNear, bool wrap = false);
float TextWidth(Graphics& g, const std::wstring& s, float px, bool bold = false);

void Fill(Graphics& g, const RectF& r, float radius, ARGB fill);
void Stroke(Graphics& g, const RectF& r, float radius, ARGB color, float width = 1.f);

void Clay(Graphics& g, const RectF& r, float radius, ARGB fill, float depth = 1.f);   // raised: outer dark+light shadow, inner highlight+shade
void Inset(Graphics& g, const RectF& r, float radius, ARGB fill);                      // sunken: inner double shadow
void Card(Graphics& g, const RectF& r, const std::wstring& title = L"");
void Bar(Graphics& g, const RectF& r, double frac01, ARGB color);

void TabIcon(Graphics& g, const std::wstring& name, const RectF& r, ARGB color);       // vector glyphs
ARGB TabColor(const std::wstring& name);
void DrawLogo(Graphics& g, const RectF& r);                                            // purple "B"
void Avatar(Graphics& g, const RectF& r, Bitmap* img, ARGB color, const std::wstring& initials);
Bitmap* AvatarImage(const std::wstring& relativePath);                                 // 256x256 cover-cropped, cached
Bitmap* Image(const std::wstring& relativePath);                                       // cached; relative to exe folder
void Forget(const std::wstring& relativePath);
void Shutdown();
}
