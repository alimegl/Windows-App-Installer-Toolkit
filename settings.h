#ifndef SETTINGS_H
#define SETTINGS_H

#include "applications.h"

#include <windows.h>

#include <array>

namespace settings {

void LoadInterface(std::array<HWND, applications.size()>& checkboxes, int& themePreference);
void SaveInterface(HWND window, const std::array<HWND, applications.size()>& checkboxes, int themePreference);
SIZE LoadWindowSize();

} // namespace settings

#endif
