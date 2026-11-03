#include "MainWindow.h"
#include "../Core/App.h"
#include "../Core/Profile.h"
#include "../Core/Util.h"
#include "../Monitoring/Monitor.h"
#include "../Permissions/Permissions.h"
#include "../Tabs/Tabs.h"
#include "../Tabs/GameMode/GameMode.h"
#include <dwmapi.h>
#include <cmath>

using namespace Gdiplus;

static const float kW = 1080, kH = 700, kSide = 250;
static const UINT kTimer = 1;
static const UINT kSplashTimer = 2;
static constexpr ULONGLONG kSplashDuration = 2350;
static constexpr ULONGLONG kSplashFadeStart = 1550;

static RectF SideItem(int i) { return RectF(28, 178 + i * 52.f, kSide - 56, 46); }
static RectF ProfileChip() { return RectF(28, 96, kSide - 56, 66); }

bool MainWindow::Create(HINSTANCE inst) {
    HDC dc = GetDC(nullptr);
    scale_ = GetDeviceCaps(dc, LOGPIXELSX) / 96.0f;
    ReleaseDC(nullptr, dc);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = Proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"BoosterikMain";
    RegisterClassExW(&wc);

    tabs_.push_back(MakeHomeTab());
    tabs_.push_back(MakeAccelerationTab());
    tabs_.push_back(MakeCleaningTab());
    tabs_.push_back(MakeProgramsTab());
    tabs_.push_back(MakeCrosshairsTab());
    tabs_.push_back(MakeColorsTab());
    tabs_.push_back(MakeDriversTab());
    tabs_.push_back(MakeSettingsTab());

    RECT rc{0, 0, (LONG)(kW * scale_), (LONG)(kH * scale_)};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;   // fixed size
    AdjustWindowRect(&rc, style, FALSE);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2, y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    hwnd_ = CreateWindowExW(0, L"BoosterikMain", L"Boosterik × SynhroBoost — Gaming Toolkit", style, x, y, w, h,
                            nullptr, nullptr, inst, this);
    if (!hwnd_) return false;

    BOOL dark = FALSE;                                           // light title bar to match the pastel theme
    DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof(dark));

    HWND hw = hwnd_;
    app::goTab = [this](int i) { SelectTab(i); };
    app::requestRepaint = [hw]() { PostMessageW(hw, WM_APP, 0, 0); };

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    splashStarted_ = GetTickCount64();
    SetTimer(hwnd_, kSplashTimer, 16, nullptr);
    SetTimer(hwnd_, kTimer, 1000, nullptr);
    return true;
}

LRESULT CALLBACK MainWindow::Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    MainWindow* self = (MainWindow*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE) {
        self = (MainWindow*)((CREATESTRUCTW*)l)->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
        if (self) self->hwnd_ = h;
    }
    return self ? self->Handle(m, w, l) : DefWindowProcW(h, m, w, l);
}

PointF MainWindow::ToLogical(LPARAM lp) const {
    return PointF((float)(short)LOWORD(lp) / scale_, (float)(short)HIWORD(lp) / scale_);
}

void MainWindow::SelectTab(int i) {
    if (i < 0 || i >= (int)tabs_.size()) return;
    cur_ = i;
    tabs_[cur_]->OnEnter();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

LRESULT MainWindow::Handle(UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        Paint();
        return 0;
    case WM_TIMER:
        if (w == kSplashTimer) {
            if (GetTickCount64() - splashStarted_ >= kSplashDuration) {
                splashActive_ = false;
                KillTimer(hwnd_, kSplashTimer);
                tabs_[cur_]->OnEnter();
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (w == kTimer) {
            Monitor::I().Update();
            GameMode::Tick();
            tabs_[cur_]->OnTick();
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    case WM_APP:
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_CHAR:
        if (!splashActive_) { tabs_[cur_]->OnChar((wchar_t)w); InvalidateRect(hwnd_, nullptr, FALSE); }
        return 0;
    case WM_LBUTTONDOWN: {
        if (splashActive_) return 0;
        PointF p = ToLogical(l);
        SetFocus(hwnd_);
        if (p.X < kSide) {
            if (ProfileChip().Contains(p)) SelectTab(app::Settings);
            for (int i = 0; i < (int)tabs_.size(); ++i)
                if (SideItem(i).Contains(p)) { SelectTab(i); break; }
        } else {
            SetCapture(hwnd_);
            tabs_[cur_]->MouseDown(p);
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (splashActive_) return 0;
        PointF p = ToLogical(l);
        int hs = -1;
        for (int i = 0; i < (int)tabs_.size(); ++i) if (SideItem(i).Contains(p)) hs = i;
        hoverSide_ = hs;
        hoverChip_ = ProfileChip().Contains(p);
        tabs_[cur_]->MouseMove(p);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONUP: {
        if (GetCapture() == hwnd_) ReleaseCapture();
        if (splashActive_) return 0;
        tabs_[cur_]->MouseUp(ToLogical(l));
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd_, kTimer);
        KillTimer(hwnd_, kSplashTimer);
        GameMode::Disable();
        ShutdownCrosshairs();
        ShutdownColors();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, m, w, l);
}

void MainWindow::PaintSplash(Graphics& g, int cw, int ch) {
    const ULONGLONG elapsed = GetTickCount64() - splashStarted_;
    float fade = 1.0f;
    if (elapsed > kSplashFadeStart)
        fade = 1.0f - (float)(elapsed - kSplashFadeStart) / (float)(kSplashDuration - kSplashFadeStart);
    fade = max(0.0f, min(1.0f, fade));

    // render the splash into its own bitmap so the whole picture can fade out smoothly
    Bitmap sb(cw, ch, PixelFormat32bppARGB);
    {
        Graphics s(&sb);
        s.SetSmoothingMode(SmoothingModeAntiAlias);
        s.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        s.ScaleTransform(scale_, scale_);
        s.Clear(Color(ui::col::bg));

        const float cx = kW / 2, cy = kH / 2 - 30;
        ui::Clay(s, RectF(cx - 76, cy - 130, 152, 152), 76, ui::col::surface, 1.2f);
        ui::DrawLogo(s, RectF(cx - 40, cy - 94, 80, 80));

        float a0 = (float)(elapsed % 1400) / 1400.0f * 360.0f;
        Pen ring1(Color(ui::col::purple), 6.f), ring2(Color(ui::col::pink), 6.f);
        ring1.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
        ring2.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
        s.DrawArc(&ring1, cx - 96, cy - 150, 192, 192, a0, 80);
        s.DrawArc(&ring2, cx - 96, cy - 150, 192, 192, a0 + 180, 50);

        ui::Text(s, L"Boosterik", RectF(cx - 200, cy + 46, 400, 44), 34, ui::col::text, true,
                 StringAlignmentCenter, StringAlignmentCenter);
        ui::Text(s, L"Підготовка оптимізатора…", RectF(cx - 200, cy + 92, 400, 24), 15, ui::col::muted, false,
                 StringAlignmentCenter, StringAlignmentCenter);
        ui::Bar(s, RectF(cx - 130, cy + 134, 260, 16), min(1.0, (double)elapsed / (double)kSplashDuration), ui::col::purple);
        ui::Text(s, L"GAME PERFORMANCE SUITE  ·  v1.0.0", RectF(cx - 200, cy + 166, 400, 20), 12, ui::col::muted, false,
                 StringAlignmentCenter, StringAlignmentCenter);
    }
    ColorMatrix matrix = {{{1, 0, 0, 0, 0}, {0, 1, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, fade, 0}, {0, 0, 0, 0, 1}}};
    ImageAttributes attrs;
    attrs.SetColorMatrix(&matrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
    g.DrawImage(&sb, RectF(0, 0, cw / scale_, ch / scale_), 0, 0, (REAL)cw, (REAL)ch, UnitPixel, &attrs);
}

void MainWindow::Paint() {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd_, &ps);
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int cw = rc.right - rc.left, ch = rc.bottom - rc.top;

    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, cw, ch);
    HGDIOBJ old = SelectObject(mem, bmp);
    {
        Graphics g(mem);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.ScaleTransform(scale_, scale_);
        g.Clear(Color(ui::col::bg));

        // ---- sidebar: one big clay panel
        ui::Card(g, RectF(16, 16, kSide - 32, kH - 32));
        ui::DrawLogo(g, RectF(36, 34, 38, 38));
        ui::Text(g, L"Boosterik", RectF(82, 32, 140, 26), 21, ui::col::text, true);
        ui::Text(g, L"× SynhroBoost", RectF(82, 58, 140, 18), 12, ui::col::purple, true);

        const ProfileData& pf = Profile::Get();
        RectF chip = ProfileChip();
        ui::Inset(g, chip, 24, hoverChip_ ? 0xFFE6DCFF : ui::col::card2);
        ARGB pc = ui::col::purple;
        switch (pf.color % 5) { case 1: pc = ui::col::pink; break; case 2: pc = ui::col::blue; break;
                                case 3: pc = ui::col::mint; break; case 4: pc = ui::col::lavender; break; }
        ui::Avatar(g, RectF(chip.X + 7, chip.Y + 7, 52, 52), ui::AvatarImage(pf.avatarRel), pc, Profile::Initials());
        ui::Text(g, Profile::Display(), RectF(chip.X + 68, chip.Y + 12, chip.Width - 76, 24), 14, ui::col::text, true,
                 StringAlignmentNear, StringAlignmentCenter);
        ui::Text(g, L"Мій профіль", RectF(chip.X + 68, chip.Y + 36, chip.Width - 76, 18), 12, ui::col::muted);

        for (int i = 0; i < (int)tabs_.size(); ++i) {
            RectF r = SideItem(i);
            bool sel = i == cur_, hot = i == hoverSide_;
            if (sel) ui::Clay(g, r, 23, ui::col::purple, 0.55f);
            else if (hot) ui::Fill(g, r, 23, ui::col::card2);
            std::wstring icon = tabs_[i]->IconName();
            RectF tile(r.X + 5, r.Y + 5, 36, 36);
            ui::Fill(g, tile, 18, ui::TabColor(icon));
            ui::Fill(g, RectF(tile.X + 7, tile.Y + 3, 22, 8), 4, ui::WithAlpha(0xFFFFFFFF, 90));
            ui::TabIcon(g, icon, RectF(tile.X + 8, tile.Y + 8, 20, 20), ui::col::text);
            ui::Text(g, tabs_[i]->Title(), RectF(r.X + 52, r.Y, r.Width - 56, r.Height), 15,
                     sel ? 0xFFFFFFFF : ui::col::text, sel, StringAlignmentNear, StringAlignmentCenter);
        }
        ui::Text(g, std::wstring(L"v1.0.0") + (Permissions::IsAdmin() ? L"  ·  адміністратор" : L""),
                 RectF(36, kH - 54, kSide - 70, 20), 12, ui::col::muted);

        // ---- content
        RectF area(kSide, 24, kW - kSide - 32, kH - 48);
        ui::Text(g, tabs_[cur_]->Title(), RectF(area.X, area.Y, area.Width, 40), 30, ui::col::text, true);
        ui::Text(g, tabs_[cur_]->Subtitle(), RectF(area.X, area.Y + 42, area.Width, 22), 14, ui::col::muted);
        tabs_[cur_]->Draw(g, RectF(area.X, area.Y + 82, area.Width - 6, area.Height - 82));

        if (splashActive_) PaintSplash(g, cw, ch);
    }
    BitBlt(dc, 0, 0, cw, ch, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd_, &ps);
}
