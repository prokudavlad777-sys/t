#include "../Tabs.h"
#include "../../Backup/Backup.h"
#include "../../Core/Profile.h"
#include "../../Core/Util.h"
#include "../../Logger/Logger.h"
#include "../../Permissions/Permissions.h"
#include "../../Restore/Restore.h"
#include <commdlg.h>
#include <shellapi.h>

using namespace Gdiplus;

namespace {
const size_t kMaxName = 24;

static std::wstring PickImage() {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW o{};
    o.lStructSize = sizeof(o);
    o.hwndOwner = GetActiveWindow();
    o.lpstrFilter = L"Зображення\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0Усі файли\0*.*\0";
    o.lpstrFile = file;
    o.nMaxFile = MAX_PATH;
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&o) ? std::wstring(file) : std::wstring();
}

static std::wstring ClipboardText() {
    std::wstring out;
    if (!OpenClipboard(nullptr)) return out;
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (const wchar_t* p = (const wchar_t*)GlobalLock(h)) { out = p; GlobalUnlock(h); }
    }
    CloseClipboard();
    return out;
}

class SettingsTab : public Tab {
    std::wstring status_, profileStatus_;
    int focus_ = 0;                      // 0 = none, 1 = first name, 2 = last name
public:
    const wchar_t* Title() const override { return L"Налаштування"; }
    const wchar_t* Subtitle() const override { return L"Профіль, права, резервні копії та журнал"; }
    const wchar_t* IconName() const override { return L"settings"; }

    void Draw(Graphics& g, const RectF& a) override {
        Begin();
        ProfileData& pf = Profile::Get();

        // ---- profile
        RectF pc(a.X, a.Y, a.Width, 204);
        ui::Card(g, pc, L"Профіль");
        ARGB swatch[5] = {ui::col::purple, ui::col::pink, ui::col::blue, ui::col::mint, ui::col::lavender};
        ui::Avatar(g, RectF(pc.X + 26, pc.Y + 56, 112, 112), ui::AvatarImage(pf.avatarRel), swatch[pf.color % 5],
                   Profile::Initials());
        float x0 = pc.X + 166, fw = (pc.Width - 166 - 26 - 14) / 2;
        ui::Text(g, L"Ім'я", RectF(x0 + 8, pc.Y + 44, fw, 18), 12, ui::col::muted);
        ui::Text(g, L"Прізвище", RectF(x0 + fw + 22, pc.Y + 44, fw, 18), 12, ui::col::muted);
        TextBox(g, RectF(x0, pc.Y + 64, fw, 46), L"Введіть ім'я", pf.first, 40, focus_ == 1);
        TextBox(g, RectF(x0 + fw + 14, pc.Y + 64, fw, 46), L"Введіть прізвище", pf.last, 41, focus_ == 2);
        Btn(g, RectF(x0, pc.Y + 124, 196, 44), L"Обрати аватарку", 50, true);
        Btn(g, RectF(x0 + 208, pc.Y + 124, 130, 44), L"Прибрати", 51, false, !pf.avatarRel.empty());
        for (int i = 0; i < 5; ++i) {
            RectF sw(pc.X + pc.Width - 26 - (5 - i) * 44 + 8, pc.Y + 130, 32, 32);
            Area(sw, 60 + i);
            ui::Clay(g, sw, 16, swatch[i], 0.3f);
            if (pf.color == i) ui::Stroke(g, RectF(sw.X - 4, sw.Y - 4, 40, 40), 20, ui::col::text, 2.f);
        }
        ui::Text(g, profileStatus_.empty() ? std::wstring(L"Без фото показуються ініціали на кольоровому фоні. Зберігається автоматично.")
                                           : profileStatus_,
                 RectF(x0, pc.Y + 174, pc.Width - 166 - 26, 22), 12, ui::col::muted);

        // ---- rights + backup
        bool admin = Permissions::IsAdmin();
        float half = (a.Width - 16) / 2;
        RectF rc(a.X, a.Y + 220, half, 176), bc(a.X + half + 16, a.Y + 220, half, 176);
        ui::Card(g, rc, L"Права доступу");
        ui::Fill(g, RectF(rc.X + 24, rc.Y + 56, 12, 12), 6, admin ? ui::col::good : ui::col::warn);
        ui::Text(g, admin ? L"Запущено від імені адміністратора"
                          : L"Звичайний режим: рівень «Високий» і Windows Temp потребують адміністратора",
                 RectF(rc.X + 46, rc.Y + 48, rc.Width - 70, 50), 13, ui::col::text, false, StringAlignmentNear,
                 StringAlignmentNear, true);
        Btn(g, RectF(rc.X + 22, rc.Y + 112, rc.Width - 44, 46), L"Запустити як адміністратор", 1, false, !admin);

        ui::Card(g, bc, L"Резервна копія");
        ui::Text(g, L"Остання копія: " + Backup::LastWriteText(), RectF(bc.X + 24, bc.Y + 46, bc.Width - 48, 22), 13, ui::col::muted);
        float bw = (bc.Width - 44 - 10) / 2;
        Btn(g, RectF(bc.X + 22, bc.Y + 76, bw, 46), L"Створити", 2, true, !Busy());
        Btn(g, RectF(bc.X + 32 + bw, bc.Y + 76, bw, 46), L"Відновити", 3, false, !Busy());
        std::wstring st;
        { std::lock_guard<std::mutex> lk(mu_); st = status_; }
        ui::Text(g, st.empty() ? std::wstring(L"Копія створюється автоматично перед першою оптимізацією.") : st,
                 RectF(bc.X + 24, bc.Y + 130, bc.Width - 48, 40), 12, ui::col::muted, false, StringAlignmentNear,
                 StringAlignmentNear, true);

        // ---- files
        RectF fc(a.X, a.Y + 412, a.Width, 104);
        ui::Card(g, fc, L"Файли");
        Btn(g, RectF(fc.X + 22, fc.Y + 50, 210, 44), L"Відкрити журнал", 4, false);
        Btn(g, RectF(fc.X + 244, fc.Y + 50, 210, 44), L"Папка програми", 5, false);
        ui::Text(g, L"Boosterik × SynhroBoost · v1.0.0", RectF(fc.X + 470, fc.Y + 50, fc.Width - 494, 44), 13,
                 ui::col::muted, false, StringAlignmentFar, StringAlignmentCenter);
    }

    void OnChar(wchar_t c) override {
        if (focus_ == 0) return;
        ProfileData& pf = Profile::Get();
        std::wstring& s = focus_ == 1 ? pf.first : pf.last;
        if (c == 8) { if (!s.empty()) s.pop_back(); }                 // Backspace
        else if (c == 13 || c == 27) { focus_ = 0; }                   // Enter / Esc
        else if (c == 9) { focus_ = focus_ == 1 ? 2 : 1; }             // Tab
        else if (c == 22) {                                            // Ctrl+V
            for (wchar_t ch : ClipboardText()) { if (ch >= 32 && s.size() < kMaxName) s.push_back(ch); }
        } else if (c >= 32 && s.size() < kMaxName) s.push_back(c);
        Profile::Save();
    }

    void OnButton(int id) override {
        ProfileData& pf = Profile::Get();
        if (id == 40) { focus_ = 1; return; }
        if (id == 41) { focus_ = 2; return; }
        focus_ = 0;
        if (id >= 60 && id < 65) { pf.color = id - 60; Profile::Save(); return; }
        if (id == 50) {
            std::wstring f = PickImage();
            if (f.empty()) return;
            ui::Forget(pf.avatarRel);
            if (Profile::SetAvatar(f)) {
                ui::Forget(pf.avatarRel);
                if (ui::AvatarImage(pf.avatarRel)) profileStatus_ = L"Аватарку збережено.";
                else { Profile::ClearAvatar(); ui::Forget(L""); profileStatus_ = L"Не вдалося прочитати це зображення."; }
            } else profileStatus_ = L"Оберіть PNG, JPG, BMP або GIF.";
        } else if (id == 51) {
            ui::Forget(pf.avatarRel);
            Profile::ClearAvatar();
            profileStatus_ = L"Аватарку прибрано.";
        } else if (id == 1) {
            if (Permissions::RelaunchAsAdmin()) PostMessageW(GetActiveWindow(), WM_CLOSE, 0, 0);
        } else if (id == 2) {
            bool ok = Backup::Create();
            std::lock_guard<std::mutex> lk(mu_);
            status_ = ok ? L"Резервну копію створено." : L"Не вдалося записати копію (немає прав на запис у папку програми?).";
        } else if (id == 3) {
            if (MessageBoxW(nullptr, L"Відновити систему зі збереженої резервної копії?", L"Boosterik",
                            MB_YESNO | MB_ICONQUESTION) != IDYES) return;
            RunAsync([this]() {
                Restore::Result r = Restore::Run();
                std::wstring s;
                for (size_t i = 0; i < r.lines.size() && i < 2; ++i) s += r.lines[i] + L" ";
                std::lock_guard<std::mutex> lk(mu_);
                status_ = s;
            });
        } else if (id == 4) {
            ShellExecuteW(nullptr, L"open", Log::FilePath().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        } else if (id == 5) {
            ShellExecuteW(nullptr, L"open", util::ExeDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }
};
}

std::unique_ptr<Tab> MakeSettingsTab() { return std::make_unique<SettingsTab>(); }
