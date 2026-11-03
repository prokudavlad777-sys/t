#pragma once
#include <string>

struct ProfileData {
    std::wstring first, last;
    std::wstring avatarRel;      // relative to the exe folder, e.g. configs\avatar.png ("" = none)
    int color = 0;               // pastel colour of the initials avatar (0..4)
};

namespace Profile {
ProfileData& Get();
void Load();
void Save();
std::wstring Display();          // "Ім'я Прізвище" or a default
std::wstring Initials();
bool SetAvatar(const std::wstring& sourceFile);   // copies the picture into configs\
void ClearAvatar();
}
