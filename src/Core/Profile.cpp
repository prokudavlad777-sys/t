#include "Profile.h"
#include "Util.h"
#include <cwctype>

namespace Profile {

static ProfileData g_p;
static std::wstring Ini() { return util::Path(L"configs\\profile.ini"); }
static const wchar_t* kExts[] = {L".png", L".jpg", L".jpeg", L".bmp", L".gif"};

ProfileData& Get() { return g_p; }

// The ini file is created as UTF-16 so Cyrillic names survive on any Windows code page.
static void EnsureIni() {
    util::EnsureDir(util::Path(L"configs"));
    std::wstring f = Ini();
    if (util::FileExists(f)) return;
    HANDLE h = CreateFileW(f.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    const BYTE bom[2] = {0xFF, 0xFE};
    DWORD w = 0;
    WriteFile(h, bom, 2, &w, nullptr);
    CloseHandle(h);
}

void Load() {
    std::wstring f = Ini();
    wchar_t b[260];
    GetPrivateProfileStringW(L"profile", L"first", L"", b, 260, f.c_str());  g_p.first = b;
    GetPrivateProfileStringW(L"profile", L"last", L"", b, 260, f.c_str());   g_p.last = b;
    GetPrivateProfileStringW(L"profile", L"avatar", L"", b, 260, f.c_str()); g_p.avatarRel = b;
    g_p.color = (int)GetPrivateProfileIntW(L"profile", L"color", 0, f.c_str()) % 5;
    if (!g_p.avatarRel.empty() && !util::FileExists(util::Path(g_p.avatarRel))) g_p.avatarRel.clear();
}

void Save() {
    EnsureIni();
    std::wstring f = Ini();
    WritePrivateProfileStringW(L"profile", L"first", g_p.first.c_str(), f.c_str());
    WritePrivateProfileStringW(L"profile", L"last", g_p.last.c_str(), f.c_str());
    WritePrivateProfileStringW(L"profile", L"avatar", g_p.avatarRel.c_str(), f.c_str());
    WritePrivateProfileStringW(L"profile", L"color", std::to_wstring(g_p.color).c_str(), f.c_str());
}

std::wstring Display() {
    std::wstring n = g_p.first;
    if (!n.empty() && !g_p.last.empty()) n += L" ";
    n += g_p.last;
    return n.empty() ? L"Гравець" : n;
}

std::wstring Initials() {
    std::wstring s;
    if (!g_p.first.empty()) s += (wchar_t)towupper(g_p.first[0]);
    if (!g_p.last.empty()) s += (wchar_t)towupper(g_p.last[0]);
    return s.empty() ? L"B" : s;
}

static void DeleteOldAvatars() {
    for (auto e : kExts) DeleteFileW(util::Path(std::wstring(L"configs\\avatar") + e).c_str());
}

bool SetAvatar(const std::wstring& src) {
    size_t d = src.find_last_of(L'.');
    if (d == std::wstring::npos) return false;
    std::wstring ext = util::ToLower(src.substr(d));
    bool ok = false;
    for (auto e : kExts) if (ext == e) ok = true;
    if (!ok) return false;
    util::EnsureDir(util::Path(L"configs"));
    DeleteOldAvatars();
    std::wstring rel = L"configs\\avatar" + ext;
    if (!CopyFileW(src.c_str(), util::Path(rel).c_str(), FALSE)) return false;
    g_p.avatarRel = rel;
    Save();
    return true;
}

void ClearAvatar() {
    DeleteOldAvatars();
    g_p.avatarRel.clear();
    Save();
}
}
