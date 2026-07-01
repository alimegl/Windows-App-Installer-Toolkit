#include "settings.h"

#include <algorithm>

namespace {

constexpr wchar_t SETTINGS_KEY[] = L"Software\\WindowsAppInstallerGui";

DWORD ReadDword(const wchar_t* name) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER, SETTINGS_KEY, name, RRF_RT_REG_DWORD, nullptr, &value, &size);
    return value;
}

} // namespace

namespace settings {

void LoadInterface(std::array<HWND, applications.size()>& checkboxes, int& themePreference) {
    const DWORD selectedMask = ReadDword(L"Selection");
    for (std::size_t index = 0; index < checkboxes.size(); ++index) {
        SendMessageW(
            checkboxes[index],
            BM_SETCHECK,
            (selectedMask & (1u << index)) != 0 ? BST_CHECKED : BST_UNCHECKED,
            0
        );
    }
    themePreference = static_cast<int>(std::min<DWORD>(ReadDword(L"Theme"), 2));
}

void SaveInterface(HWND window, const std::array<HWND, applications.size()>& checkboxes, int themePreference) {
    HKEY key{};
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            SETTINGS_KEY,
            0,
            nullptr,
            0,
            KEY_WRITE,
            nullptr,
            &key,
            nullptr) != ERROR_SUCCESS) {
        return;
    }

    DWORD selectedMask = 0;
    for (std::size_t index = 0; index < checkboxes.size(); ++index) {
        if (SendMessageW(checkboxes[index], BM_GETCHECK, 0, 0) == BST_CHECKED) {
            selectedMask |= 1u << index;
        }
    }

    RECT bounds{};
    GetWindowRect(window, &bounds);
    const DWORD values[] = {
        selectedMask,
        static_cast<DWORD>(themePreference),
        static_cast<DWORD>(bounds.right - bounds.left),
        static_cast<DWORD>(bounds.bottom - bounds.top)
    };
    const wchar_t* names[] = {L"Selection", L"Theme", L"Width", L"Height"};
    for (std::size_t index = 0; index < std::size(values); ++index) {
        RegSetValueExW(
            key,
            names[index],
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE*>(&values[index]),
            sizeof(values[index])
        );
    }
    RegCloseKey(key);
}

SIZE LoadWindowSize() {
    return SIZE{
        static_cast<LONG>(ReadDword(L"Width")),
        static_cast<LONG>(ReadDword(L"Height"))
    };
}

} // namespace settings
