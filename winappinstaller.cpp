#include "winappinstaller.h"
#include "resource.h"

#include <dwmapi.h>
#include <commctrl.h>
#include <shellapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cwctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Application {
    const wchar_t* name;
    const wchar_t* packageId;
    const wchar_t* source;
    int profile;
};

constexpr std::array<Application, 13> applications{{
    {L"Discord", L"Discord.Discord", L"winget", 1},
    {L"WhatsApp", L"9NKSQGP7F2NH", L"msstore", 1},
    {L"Apple Music", L"9PFHDD62MXS1", L"msstore", 1},
    {L"Streamlabs OBS", L"Streamlabs.Streamlabs", L"winget", 2},
    {L"Steam", L"Valve.Steam", L"winget", 2},
    {L"Ubisoft Connect", L"Ubisoft.Connect", L"winget", 2},
    {L"EA App", L"ElectronicArts.EADesktop", L"winget", 2},
    {L"Epic Games Launcher", L"EpicGames.EpicGamesLauncher", L"winget", 2},
    {L"Rockstar Games Launcher", L"RockstarGames.Launcher", L"winget", 2},
    {L"Visual Studio Code", L"Microsoft.VisualStudioCode", L"winget", 3},
    {L"Google Chrome", L"Google.Chrome", L"winget", 1},
    {L"MSYS2", L"MSYS2.MSYS2", L"winget", 3},
    {L"Git", L"Git.Git", L"winget", 3},
}};

constexpr int ID_CHECKBOX_FIRST = 1000;
constexpr int ID_SELECT_ALL = 2000;
constexpr int ID_CLEAR = 2001;
constexpr int ID_INSTALL = 2002;
constexpr int ID_UPDATE = 2003;
constexpr int ID_UNINSTALL = 2004;
constexpr int ID_CANCEL = 2005;
constexpr int ID_SEARCH = 2007;
constexpr int ID_PROFILE = 2009;
constexpr int ID_REFRESH = 2011;
constexpr int ID_THEME = 2016;
constexpr int ID_MENU_OPTIONS = 3000;
constexpr int ID_MENU_APP_UPDATE = 3001;
constexpr int ID_MENU_INFO = 3002;
constexpr int ID_OPTIONS_APPLY = 3003;
constexpr int ID_OPTIONS_CLOSE = 3004;
constexpr int ID_INFO_CLOSE = 3005;
constexpr UINT WM_APP_STATUS = WM_APP + 1;
constexpr UINT WM_APP_FINISHED = WM_APP + 2;
constexpr UINT WM_APP_SCAN_FINISHED = WM_APP + 3;
constexpr UINT WM_APP_PROGRESS = WM_APP + 4;

enum class Operation { Scan, Install, Update, Uninstall };

constexpr COLORREF LIGHT_BACKGROUND_COLOR = RGB(245, 247, 250);
constexpr COLORREF LIGHT_TEXT_COLOR = RGB(28, 35, 48);
constexpr COLORREF LIGHT_MUTED_COLOR = RGB(91, 101, 117);
constexpr COLORREF DARK_BACKGROUND_COLOR = RGB(32, 32, 32);
constexpr COLORREF DARK_TEXT_COLOR = RGB(243, 243, 243);
constexpr COLORREF DARK_MUTED_COLOR = RGB(184, 188, 194);
constexpr int BASE_CLIENT_WIDTH = 820;
constexpr int BASE_CLIENT_HEIGHT = 810;
constexpr int MIN_CLIENT_WIDTH = 820;
constexpr int MIN_CLIENT_HEIGHT = 720;

std::array<HWND, applications.size()> checkboxHandles{};
HWND titleLabel = nullptr;
HWND descriptionLabel = nullptr;
HWND sectionLabel = nullptr;
HWND hintLabel = nullptr;
HWND installButton = nullptr;
HWND updateButton = nullptr;
HWND uninstallButton = nullptr;
HWND cancelButton = nullptr;
HWND selectAllButton = nullptr;
HWND clearButton = nullptr;
HWND statusLabel = nullptr;
HWND searchBox = nullptr;
HWND profileBox = nullptr;
HWND progressBar = nullptr;
HWND customPackageBox = nullptr;
HWND themeBox = nullptr;
HWND refreshButton = nullptr;
HWND optionsWindow = nullptr;
HWND infoWindow = nullptr;
HMENU mainMenuBar = nullptr;
HMENU optionsPopupMenu = nullptr;
HMENU helpPopupMenu = nullptr;
HFONT titleFont = nullptr;
HFONT normalFont = nullptr;
HFONT smallFont = nullptr;
HFONT menuFont = nullptr;
HBRUSH backgroundBrush = nullptr;
std::atomic_bool installationRunning{false};
std::atomic_bool cancellationRequested{false};
std::atomic<HANDLE> runningProcess{nullptr};
std::array<bool, applications.size()> installedApplications{};
std::array<bool, applications.size()> updateableApplications{};
std::atomic_bool restartRequired{false};
bool darkModeEnabled = false;
int themePreference = 0;
COLORREF backgroundColor = LIGHT_BACKGROUND_COLOR;
COLORREF textColor = LIGHT_TEXT_COLOR;
COLORREF mutedTextColor = LIGHT_MUTED_COLOR;

enum class PreferredAppMode {
    Default,
    AllowDark,
    ForceDark,
    ForceLight,
    Max
};

void SetNativeAppMode(PreferredAppMode mode) {
    HMODULE themeLibrary = LoadLibraryW(L"uxtheme.dll");
    if (themeLibrary == nullptr) {
        return;
    }
    using SetPreferredAppModeFunction = PreferredAppMode(WINAPI*)(PreferredAppMode);
    const FARPROC procedure = GetProcAddress(themeLibrary, MAKEINTRESOURCEA(135));
    SetPreferredAppModeFunction setPreferredAppMode = nullptr;
    static_assert(sizeof(setPreferredAppMode) == sizeof(procedure));
    memcpy(&setPreferredAppMode, &procedure, sizeof(setPreferredAppMode));
    if (setPreferredAppMode != nullptr) {
        setPreferredAppMode(mode);
    }

    using FlushMenuThemesFunction = void(WINAPI*)();
    const FARPROC flushProcedure = GetProcAddress(themeLibrary, MAKEINTRESOURCEA(136));
    FlushMenuThemesFunction flushMenuThemes = nullptr;
    static_assert(sizeof(flushMenuThemes) == sizeof(flushProcedure));
    memcpy(&flushMenuThemes, &flushProcedure, sizeof(flushMenuThemes));
    if (flushMenuThemes != nullptr) {
        flushMenuThemes();
    }
}

struct InstallJob {
    HWND window;
    std::vector<int> selectedApplications;
    Operation operation;
    std::wstring customPackage;
};

void SetFont(HWND control, HFONT font) {
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

int DpiScale(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
}

void SetControlBounds(HWND control, int x, int y, int width, int height, UINT dpi) {
    SetWindowPos(
        control,
        nullptr,
        DpiScale(x, dpi),
        DpiScale(y, dpi),
        DpiScale(width, dpi),
        DpiScale(height, dpi),
        SWP_NOACTIVATE | SWP_NOZORDER
    );
}

HWND AddLabel(HWND parent, const wchar_t* text, HFONT font) {
    HWND label = CreateWindowExW(
        0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, parent, nullptr, GetModuleHandleW(nullptr), nullptr
    );
    SetFont(label, font);
    return label;
}

HFONT CreateUiFont(int height, int weight, UINT dpi) {
    return CreateFontW(
        -DpiScale(height, dpi), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
}

void UpdateFonts(UINT dpi) {
    HFONT oldTitleFont = titleFont;
    HFONT oldNormalFont = normalFont;
    HFONT oldSmallFont = smallFont;

    titleFont = CreateUiFont(28, FW_SEMIBOLD, dpi);
    normalFont = CreateUiFont(17, FW_NORMAL, dpi);
    smallFont = CreateUiFont(15, FW_NORMAL, dpi);

    if (titleLabel != nullptr) {
        SetFont(titleLabel, titleFont);
        SetFont(descriptionLabel, smallFont);
        SetFont(sectionLabel, smallFont);
        SetFont(hintLabel, smallFont);
        SetFont(statusLabel, normalFont);
        SetFont(selectAllButton, normalFont);
        SetFont(clearButton, normalFont);
        SetFont(installButton, normalFont);
        SetFont(updateButton, normalFont);
        SetFont(uninstallButton, normalFont);
        SetFont(cancelButton, normalFont);
        for (HWND checkbox : checkboxHandles) {
            SetFont(checkbox, normalFont);
        }
    }

    DeleteObject(oldTitleFont);
    DeleteObject(oldNormalFont);
    DeleteObject(oldSmallFont);
}

void LayoutInterface(UINT dpi) {
    RECT clientBounds{};
    GetClientRect(GetParent(titleLabel), &clientBounds);
    const int clientWidth = MulDiv(clientBounds.right, USER_DEFAULT_SCREEN_DPI, static_cast<int>(dpi));
    const int clientHeight = MulDiv(clientBounds.bottom, USER_DEFAULT_SCREEN_DPI, static_cast<int>(dpi));
    const int contentWidth = clientWidth - 68;
    const int columnWidth = (contentWidth - 25) / 2;

    SetControlBounds(titleLabel, 32, 20, contentWidth, 40, dpi);
    SetControlBounds(descriptionLabel, 34, 61, contentWidth, 25, dpi);
    SetControlBounds(searchBox, 34, 100, std::max(180, contentWidth - 210), 32, dpi);
    SetControlBounds(profileBox, clientWidth - 224, 100, 190, 200, dpi);
    SetControlBounds(sectionLabel, 34, 145, 200, 22, dpi);

    int visibleIndex = 0;
    for (std::size_t index = 0; index < applications.size(); ++index) {
        if ((GetWindowLongPtrW(checkboxHandles[index], GWL_STYLE) & WS_VISIBLE) == 0) {
            continue;
        }
        const int column = visibleIndex % 2;
        const int row = visibleIndex / 2;
        SetControlBounds(
            checkboxHandles[index],
            34 + column * (columnWidth + 25),
            173 + row * 37,
            columnWidth,
            28,
            dpi
        );
        ++visibleIndex;
    }

    const int actionY = std::min(443, clientHeight - 244);
    SetControlBounds(selectAllButton, 34, actionY, 130, 34, dpi);
    SetControlBounds(clearButton, 174, actionY, 130, 34, dpi);
    SetControlBounds(installButton, 34, actionY + 47, 180, 38, dpi);
    SetControlBounds(updateButton, 224, actionY + 47, 180, 38, dpi);
    SetControlBounds(uninstallButton, 414, actionY + 47, 180, 38, dpi);
    SetControlBounds(cancelButton, clientWidth - 164, actionY + 47, 130, 38, dpi);
    SetControlBounds(progressBar, 34, actionY + 98, contentWidth, 20, dpi);
    SetControlBounds(statusLabel, 34, actionY + 128, contentWidth, 28, dpi);
    SetControlBounds(hintLabel, 34, actionY + 160, contentWidth, 22, dpi);
    SetControlBounds(customPackageBox, 34, actionY + 194, contentWidth, 30, dpi);
    SetControlBounds(refreshButton, clientWidth - 164, actionY, 130, 34, dpi);
}

bool IsWindowsDarkModeEnabled() {
    DWORD appsUseLightTheme = 1;
    DWORD valueSize = sizeof(appsUseLightTheme);
    const LSTATUS result = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD,
        nullptr,
        &appsUseLightTheme,
        &valueSize
    );
    return result == ERROR_SUCCESS && appsUseLightTheme == 0;
}

void UpdateThemeColors(bool useDarkMode) {
    backgroundColor = useDarkMode ? DARK_BACKGROUND_COLOR : LIGHT_BACKGROUND_COLOR;
    textColor = useDarkMode ? DARK_TEXT_COLOR : LIGHT_TEXT_COLOR;
    mutedTextColor = useDarkMode ? DARK_MUTED_COLOR : LIGHT_MUTED_COLOR;
}

void ApplyTheme(HWND window) {
    darkModeEnabled = themePreference == 2 || (themePreference <= 0 && IsWindowsDarkModeEnabled());
    SetNativeAppMode(themePreference == 2 ? PreferredAppMode::ForceDark
        : themePreference == 1 ? PreferredAppMode::ForceLight : PreferredAppMode::AllowDark);
    UpdateThemeColors(darkModeEnabled);

    HBRUSH newBackgroundBrush = CreateSolidBrush(backgroundColor);
    if (newBackgroundBrush != nullptr) {
        HBRUSH oldBackgroundBrush = backgroundBrush;
        backgroundBrush = newBackgroundBrush;
        SetClassLongPtrW(window, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(backgroundBrush));
        DeleteObject(oldBackgroundBrush);
    }

    const BOOL useDarkTitleBar = darkModeEnabled ? TRUE : FALSE;
    constexpr DWORD immersiveDarkModeAttribute = 20;
    constexpr DWORD legacyImmersiveDarkModeAttribute = 19;
    if (FAILED(DwmSetWindowAttribute(
            window,
            immersiveDarkModeAttribute,
            &useDarkTitleBar,
            sizeof(useDarkTitleBar)))) {
        DwmSetWindowAttribute(
            window,
            legacyImmersiveDarkModeAttribute,
            &useDarkTitleBar,
            sizeof(useDarkTitleBar)
        );
    }

    const wchar_t* controlTheme = darkModeEnabled ? L"DarkMode_Explorer" : L"Explorer";
    SetWindowTheme(window, controlTheme, nullptr);
    EnumChildWindows(window, [](HWND child, LPARAM theme) -> BOOL {
        wchar_t className[32]{};
        GetClassNameW(child, className, static_cast<int>(std::size(className)));
        if (wcscmp(className, L"Button") == 0) {
            SetWindowTheme(child, reinterpret_cast<const wchar_t*>(theme), nullptr);
            InvalidateRect(child, nullptr, TRUE);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(controlTheme));

    for (HWND comboBox : {profileBox, themeBox}) {
        if (comboBox == nullptr) {
            continue;
        }
        SetWindowTheme(comboBox, darkModeEnabled ? L"DarkMode_CFD" : L"Explorer", nullptr);
        COMBOBOXINFO comboInfo{};
        comboInfo.cbSize = sizeof(comboInfo);
        if (GetComboBoxInfo(comboBox, &comboInfo)) {
            SetWindowTheme(comboInfo.hwndList, darkModeEnabled ? L"DarkMode_Explorer" : L"Explorer", nullptr);
            InvalidateRect(comboInfo.hwndList, nullptr, TRUE);
        }
        InvalidateRect(comboBox, nullptr, TRUE);
    }

    if (progressBar != nullptr) {
        if (darkModeEnabled) {
            SetWindowTheme(progressBar, L"", L"");
            SendMessageW(progressBar, PBM_SETBKCOLOR, 0, RGB(48, 48, 48));
            SendMessageW(progressBar, PBM_SETBARCOLOR, 0, RGB(0, 120, 212));
        } else {
            SetWindowTheme(progressBar, L"Explorer", nullptr);
            SendMessageW(progressBar, PBM_SETBKCOLOR, 0, CLR_DEFAULT);
            SendMessageW(progressBar, PBM_SETBARCOLOR, 0, CLR_DEFAULT);
        }
        InvalidateRect(progressBar, nullptr, TRUE);
    }

    if (mainMenuBar != nullptr) {
        MENUINFO menuInfo{};
        menuInfo.cbSize = sizeof(menuInfo);
        menuInfo.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
        menuInfo.hbrBack = darkModeEnabled ? backgroundBrush : GetSysColorBrush(COLOR_MENU);
        SetMenuInfo(mainMenuBar, &menuInfo);
        DrawMenuBar(window);
    }

    RedrawWindow(window, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

LRESULT DrawDarkButton(const NMCUSTOMDRAW* customDraw) {
    if (!darkModeEnabled || customDraw->dwDrawStage != CDDS_PREPAINT) {
        return CDRF_DODEFAULT;
    }

    const HWND button = customDraw->hdr.hwndFrom;
    const HDC dc = customDraw->hdc;
    RECT bounds = customDraw->rc;
    const LONG_PTR style = GetWindowLongPtrW(button, GWL_STYLE);
    const LONG_PTR buttonType = style & BS_TYPEMASK;
    const bool isCheckbox = buttonType == BS_CHECKBOX || buttonType == BS_AUTOCHECKBOX
        || buttonType == BS_3STATE || buttonType == BS_AUTO3STATE;
    const bool isRadio = buttonType == BS_RADIOBUTTON || buttonType == BS_AUTORADIOBUTTON;
    const bool disabled = (customDraw->uItemState & CDIS_DISABLED) != 0;
    const bool pressed = (customDraw->uItemState & CDIS_SELECTED) != 0;
    const bool hot = (customDraw->uItemState & CDIS_HOT) != 0;

    HBRUSH background = CreateSolidBrush(DARK_BACKGROUND_COLOR);
    FillRect(dc, &bounds, background);
    DeleteObject(background);

    HFONT font = reinterpret_cast<HFONT>(SendMessageW(button, WM_GETFONT, 0, 0));
    HFONT oldFont = font == nullptr ? nullptr : reinterpret_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, disabled ? DARK_MUTED_COLOR : DARK_TEXT_COLOR);

    wchar_t caption[256]{};
    GetWindowTextW(button, caption, static_cast<int>(std::size(caption)));

    if (isCheckbox || isRadio) {
        const int boxSize = DpiScale(17, GetDpiForWindow(button));
        RECT box{bounds.left + 2, bounds.top + (bounds.bottom - bounds.top - boxSize) / 2,
            bounds.left + 2 + boxSize, bounds.top + (bounds.bottom - bounds.top + boxSize) / 2};
        HBRUSH boxBrush = CreateSolidBrush(hot ? RGB(62, 62, 62) : RGB(45, 45, 45));
        HPEN borderPen = CreatePen(PS_SOLID, 1, disabled ? RGB(82, 82, 82) : RGB(145, 145, 145));
        HGDIOBJ oldBrush = SelectObject(dc, boxBrush);
        HGDIOBJ oldPen = SelectObject(dc, borderPen);
        if (isRadio) {
            Ellipse(dc, box.left, box.top, box.right, box.bottom);
        } else {
            Rectangle(dc, box.left, box.top, box.right, box.bottom);
        }

        if (SendMessageW(button, BM_GETCHECK, 0, 0) != BST_UNCHECKED) {
            HBRUSH accentBrush = CreateSolidBrush(disabled ? RGB(70, 92, 112) : RGB(0, 120, 212));
            SelectObject(dc, accentBrush);
            if (isRadio) {
                const int inset = std::max(3, boxSize / 4);
                Ellipse(dc, box.left + inset, box.top + inset, box.right - inset, box.bottom - inset);
            } else {
                Rectangle(dc, box.left, box.top, box.right, box.bottom);
                HPEN checkPen = CreatePen(PS_SOLID, std::max(2, boxSize / 8), RGB(255, 255, 255));
                SelectObject(dc, checkPen);
                MoveToEx(dc, box.left + boxSize / 5, box.top + boxSize / 2, nullptr);
                LineTo(dc, box.left + boxSize * 2 / 5, box.bottom - boxSize / 4);
                LineTo(dc, box.right - boxSize / 5, box.top + boxSize / 4);
                SelectObject(dc, borderPen);
                DeleteObject(checkPen);
            }
            SelectObject(dc, boxBrush);
            DeleteObject(accentBrush);
        }

        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(boxBrush);
        DeleteObject(borderPen);

        RECT textBounds = bounds;
        textBounds.left = box.right + DpiScale(8, GetDpiForWindow(button));
        DrawTextW(dc, caption, -1, &textBounds, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        const COLORREF faceColor = disabled ? RGB(43, 43, 43)
            : pressed ? RGB(74, 74, 74) : hot ? RGB(62, 62, 62) : RGB(50, 50, 50);
        HBRUSH faceBrush = CreateSolidBrush(faceColor);
        HPEN borderPen = CreatePen(PS_SOLID, 1, pressed ? RGB(130, 130, 130) : RGB(92, 92, 92));
        HGDIOBJ oldBrush = SelectObject(dc, faceBrush);
        HGDIOBJ oldPen = SelectObject(dc, borderPen);
        RoundRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom,
            DpiScale(6, GetDpiForWindow(button)), DpiScale(6, GetDpiForWindow(button)));
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(faceBrush);
        DeleteObject(borderPen);
        DrawTextW(dc, caption, -1, &bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if ((customDraw->uItemState & CDIS_FOCUS) != 0) {
            RECT focusBounds = bounds;
            InflateRect(&focusBounds, -3, -3);
            DrawFocusRect(dc, &focusBounds);
        }
    }

    if (oldFont != nullptr) {
        SelectObject(dc, oldFont);
    }
    return CDRF_SKIPDEFAULT;
}

void DrawDarkComboItem(const DRAWITEMSTRUCT* drawItem) {
    if (drawItem->CtlType != ODT_COMBOBOX) {
        return;
    }

    const bool disabled = (drawItem->itemState & ODS_DISABLED) != 0;
    const bool selected = (drawItem->itemState & ODS_SELECTED) != 0;
    const COLORREF background = darkModeEnabled
        ? (selected ? RGB(0, 90, 158) : RGB(45, 45, 45))
        : (selected ? GetSysColor(COLOR_HIGHLIGHT) : GetSysColor(COLOR_WINDOW));
    const COLORREF foreground = darkModeEnabled
        ? (disabled ? DARK_MUTED_COLOR : DARK_TEXT_COLOR)
        : GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT);

    HBRUSH brush = CreateSolidBrush(background);
    FillRect(drawItem->hDC, &drawItem->rcItem, brush);
    DeleteObject(brush);

    int itemIndex = static_cast<int>(drawItem->itemID);
    if (itemIndex < 0) {
        itemIndex = static_cast<int>(SendMessageW(drawItem->hwndItem, CB_GETCURSEL, 0, 0));
    }
    if (itemIndex >= 0) {
        wchar_t text[256]{};
        SendMessageW(drawItem->hwndItem, CB_GETLBTEXT, itemIndex, reinterpret_cast<LPARAM>(text));
        RECT textBounds = drawItem->rcItem;
        textBounds.left += DpiScale(8, GetDpiForWindow(drawItem->hwndItem));
        textBounds.right -= DpiScale(5, GetDpiForWindow(drawItem->hwndItem));
        SetBkMode(drawItem->hDC, TRANSPARENT);
        SetTextColor(drawItem->hDC, foreground);
        HFONT font = reinterpret_cast<HFONT>(SendMessageW(drawItem->hwndItem, WM_GETFONT, 0, 0));
        HFONT oldFont = font == nullptr ? nullptr : reinterpret_cast<HFONT>(SelectObject(drawItem->hDC, font));
        DrawTextW(drawItem->hDC, text, -1, &textBounds, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (oldFont != nullptr) {
            SelectObject(drawItem->hDC, oldFont);
        }
    }

    if ((drawItem->itemState & ODS_FOCUS) != 0) {
        RECT focusBounds = drawItem->rcItem;
        InflateRect(&focusBounds, -2, -2);
        DrawFocusRect(drawItem->hDC, &focusBounds);
    }
}

void MeasureMenuItem(MEASUREITEMSTRUCT* measureItem) {
    const wchar_t* caption = reinterpret_cast<const wchar_t*>(measureItem->itemData);
    if (caption == nullptr) {
        measureItem->itemWidth = 80;
        measureItem->itemHeight = 24;
        return;
    }

    HDC dc = GetDC(nullptr);
    HFONT oldFont = menuFont == nullptr ? nullptr : reinterpret_cast<HFONT>(SelectObject(dc, menuFont));
    SIZE textSize{};
    GetTextExtentPoint32W(dc, caption, static_cast<int>(wcslen(caption)), &textSize);
    if (oldFont != nullptr) {
        SelectObject(dc, oldFont);
    }
    ReleaseDC(nullptr, dc);
    measureItem->itemWidth = static_cast<UINT>(textSize.cx + GetSystemMetrics(SM_CXMENUCHECK));
    measureItem->itemHeight = static_cast<UINT>(GetSystemMetrics(SM_CYMENU));
}

void DrawMenuItem(const DRAWITEMSTRUCT* drawItem) {
    const bool selected = (drawItem->itemState & ODS_SELECTED) != 0;
    const bool disabled = (drawItem->itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;
    const COLORREF background = darkModeEnabled
        ? (selected ? RGB(62, 62, 62) : DARK_BACKGROUND_COLOR)
        : GetSysColor(selected ? COLOR_HIGHLIGHT : COLOR_MENU);
    const COLORREF foreground = darkModeEnabled
        ? (disabled ? DARK_MUTED_COLOR : DARK_TEXT_COLOR)
        : GetSysColor(disabled ? COLOR_GRAYTEXT : (selected ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT));

    HBRUSH brush = CreateSolidBrush(background);
    FillRect(drawItem->hDC, &drawItem->rcItem, brush);
    DeleteObject(brush);

    const wchar_t* caption = reinterpret_cast<const wchar_t*>(drawItem->itemData);
    if (caption == nullptr) {
        return;
    }
    HFONT oldFont = menuFont == nullptr ? nullptr : reinterpret_cast<HFONT>(SelectObject(drawItem->hDC, menuFont));
    SetBkMode(drawItem->hDC, TRANSPARENT);
    SetTextColor(drawItem->hDC, foreground);
    RECT textBounds = drawItem->rcItem;
    textBounds.left += GetSystemMetrics(SM_CXEDGE) + 4;
    textBounds.right -= GetSystemMetrics(SM_CXEDGE) + 4;
    DrawTextW(drawItem->hDC, caption, -1, &textBounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (oldFont != nullptr) {
        SelectObject(drawItem->hDC, oldFont);
    }
}

void PostStatus(HWND window, std::wstring text) {
    PostMessageW(window, WM_APP_STATUS, 0, reinterpret_cast<LPARAM>(new std::wstring(std::move(text))));
}

std::wstring DecodeConsoleOutput(const char* bytes, int length) {
    int characterCount = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, length, nullptr, 0);
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;

    if (characterCount == 0) {
        codePage = GetOEMCP();
        flags = 0;
        characterCount = MultiByteToWideChar(codePage, flags, bytes, length, nullptr, 0);
    }

    if (characterCount == 0) {
        return {};
    }

    std::wstring text(static_cast<std::size_t>(characterCount), L'\0');
    MultiByteToWideChar(codePage, flags, bytes, length, text.data(), characterCount);
    return text;
}

bool IsWingetAvailable() {
    return SearchPathW(nullptr, L"winget.exe", nullptr, 0, nullptr, nullptr) > 0;
}

bool RunCommand(HWND window, const std::wstring& arguments, DWORD& exitCode, std::wstring* capturedOutput = nullptr) {
    (void)window;
    std::wstring commandLine = L"winget.exe " + arguments;
    std::vector<wchar_t> writableCommand(commandLine.begin(), commandLine.end());
    writableCommand.push_back(L'\0');

    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &securityAttributes, 0)) {
        exitCode = GetLastError();
        return false;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    HANDLE nullInput = CreateFileW(
        L"NUL",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &securityAttributes,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    startupInfo.wShowWindow = SW_HIDE;
    startupInfo.hStdInput = nullInput;
    startupInfo.hStdOutput = writePipe;
    startupInfo.hStdError = writePipe;

    PROCESS_INFORMATION processInfo{};
    const BOOL processCreated = CreateProcessW(
        nullptr,
        writableCommand.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo
    );

    CloseHandle(writePipe);
    if (nullInput != INVALID_HANDLE_VALUE) {
        CloseHandle(nullInput);
    }

    if (!processCreated) {
        exitCode = GetLastError();
        CloseHandle(readPipe);
        return false;
    }

    CloseHandle(processInfo.hThread);
    runningProcess.store(processInfo.hProcess);

    std::array<char, 4096> outputBuffer{};
    DWORD bytesRead = 0;
    while (ReadFile(readPipe, outputBuffer.data(), static_cast<DWORD>(outputBuffer.size()), &bytesRead, nullptr)
           && bytesRead > 0) {
        const std::wstring output = DecodeConsoleOutput(outputBuffer.data(), static_cast<int>(bytesRead));
        if (capturedOutput != nullptr) {
            *capturedOutput += output;
        }
    }
    CloseHandle(readPipe);

    WaitForSingleObject(processInfo.hProcess, INFINITE);
    const bool receivedExitCode = GetExitCodeProcess(processInfo.hProcess, &exitCode) != FALSE;
    runningProcess.store(nullptr);
    CloseHandle(processInfo.hProcess);

    if (exitCode == 3010 || exitCode == 1641 || exitCode == 0x8A15010B) {
        restartRequired = true;
    }
    return receivedExitCode && exitCode == 0;
}

DWORD WINAPI InstallWorker(void* parameter) {
    InstallJob* job = static_cast<InstallJob*>(parameter);
    DWORD exitCode = 0;

    if (job->operation == Operation::Scan) {
        std::wstring listed;
        std::wstring upgrades;
        PostStatus(job->window, L"Installierte Apps und Updates werden geprüft …");
        RunCommand(job->window, L"list --accept-source-agreements --disable-interactivity", exitCode, &listed);
        RunCommand(job->window, L"upgrade --accept-source-agreements --disable-interactivity", exitCode, &upgrades);
        std::transform(listed.begin(), listed.end(), listed.begin(), towlower);
        std::transform(upgrades.begin(), upgrades.end(), upgrades.begin(), towlower);
        for (std::size_t index = 0; index < applications.size(); ++index) {
            std::wstring id = applications[index].packageId;
            std::transform(id.begin(), id.end(), id.begin(), towlower);
            installedApplications[index] = listed.find(id) != std::wstring::npos;
            updateableApplications[index] = upgrades.find(id) != std::wstring::npos;
        }
        PostMessageW(job->window, WM_APP_SCAN_FINISHED, 0, 0);
        delete job;
        return 0;
    }

    if (job->operation == Operation::Install) {
        PostStatus(job->window, L"Winget-Paketquelle wird aktualisiert …");
        RunCommand(job->window, L"source update --name winget --disable-interactivity", exitCode);
    }

    int successful = 0;
    const int total = static_cast<int>(job->selectedApplications.size()) + (job->customPackage.empty() ? 0 : 1);
    int current = 0;
    auto runPackage = [&](const std::wstring& name, const std::wstring& id, const std::wstring& source) {
        if (cancellationRequested.load()) {
            return;
        }
        ++current;
        PostMessageW(job->window, WM_APP_PROGRESS, current, total);
        const wchar_t* verb = job->operation == Operation::Install ? L"Installiere "
            : job->operation == Operation::Update ? L"Aktualisiere " : L"Deinstalliere ";
        PostStatus(job->window, verb + name + L" …");
        std::wstring arguments = job->operation == Operation::Install ? L"install" :
            job->operation == Operation::Update ? L"upgrade" : L"uninstall";
        arguments += L" -e --id \"" + id + L"\"";
        if (!source.empty() && job->operation != Operation::Uninstall) {
            arguments += L" --source \"" + source + L"\"";
        }
        arguments += L" --accept-source-agreements --disable-interactivity";
        if (job->operation != Operation::Uninstall) {
            arguments += L" --accept-package-agreements";
        }
        if (RunCommand(job->window, arguments, exitCode)) {
            ++successful;
        }
    };

    for (int index : job->selectedApplications) {
        const Application& application = applications[static_cast<std::size_t>(index)];
        runPackage(application.name, application.packageId, application.source);
    }
    if (!job->customPackage.empty()) {
        runPackage(job->customPackage, job->customPackage, L"");
    }

    PostMessageW(
        job->window,
        WM_APP_FINISHED,
        static_cast<WPARAM>(successful),
        static_cast<LPARAM>(total)
    );
    delete job;
    return 0;
}

void SetInstallerControlsEnabled(bool enabled) {
    for (HWND checkbox : checkboxHandles) {
        EnableWindow(checkbox, enabled);
    }
    EnableWindow(selectAllButton, enabled);
    EnableWindow(clearButton, enabled);
    EnableWindow(installButton, enabled);
    EnableWindow(updateButton, enabled);
    EnableWindow(uninstallButton, enabled);
    EnableWindow(refreshButton, enabled);
    EnableWindow(cancelButton, !enabled);
}

void SelectAll(bool selected) {
    for (HWND checkbox : checkboxHandles) {
        SendMessageW(checkbox, BM_SETCHECK, selected ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

void StartOperation(HWND window, Operation operation) {
    if (!IsWingetAvailable()) {
        MessageBoxW(
            window,
            L"Winget wurde nicht gefunden. Bitte installiere oder aktualisiere zuerst den Windows App Installer.",
            L"Winget fehlt",
            MB_OK | MB_ICONERROR
        );
        return;
    }

    std::vector<int> selected;
    for (std::size_t index = 0; index < checkboxHandles.size(); ++index) {
        if (SendMessageW(checkboxHandles[index], BM_GETCHECK, 0, 0) == BST_CHECKED) {
            selected.push_back(static_cast<int>(index));
        }
    }

    wchar_t customId[256]{};
    GetWindowTextW(customPackageBox, customId, 256);

    if (operation != Operation::Scan && selected.empty() && customId[0] == L'\0') {
        MessageBoxW(window, L"Bitte wähle mindestens eine Anwendung aus.", L"Keine Auswahl", MB_OK | MB_ICONINFORMATION);
        return;
    }

    installationRunning = true;
    restartRequired = false;
    cancellationRequested = false;
    SetInstallerControlsEnabled(false);
    SendMessageW(progressBar, PBM_SETPOS, 0, 0);
    SetWindowTextW(statusLabel, operation == Operation::Scan ? L"Prüfung wird vorbereitet …" : L"Auftrag wird vorbereitet …");

    InstallJob* job = new InstallJob{
        window,
        std::move(selected),
        operation,
        customId
    };
    HANDLE thread = CreateThread(nullptr, 0, InstallWorker, job, 0, nullptr);
    if (thread == nullptr) {
        delete job;
        installationRunning = false;
        SetInstallerControlsEnabled(true);
        SetWindowTextW(statusLabel, L"Die Installation konnte nicht gestartet werden.");
        return;
    }
    CloseHandle(thread);
}

std::wstring GetControlText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    if (length > 0) {
        GetWindowTextW(control, text.data(), length + 1);
    }
    text.resize(static_cast<std::size_t>(length));
    return text;
}

void UpdateApplicationLabels() {
    for (std::size_t index = 0; index < applications.size(); ++index) {
        std::wstring label = applications[index].name;
        if (updateableApplications[index]) {
            label += L"  • Update verfügbar";
        } else if (installedApplications[index]) {
            label += L"  • Installiert";
        }
        SetWindowTextW(checkboxHandles[index], label.c_str());
    }
}

void ApplyFilters(HWND window) {
    std::wstring search = GetControlText(searchBox);
    std::transform(search.begin(), search.end(), search.begin(), towlower);
    for (std::size_t index = 0; index < applications.size(); ++index) {
        std::wstring searchable = std::wstring(applications[index].name) + L" " + applications[index].packageId;
        std::transform(searchable.begin(), searchable.end(), searchable.begin(), towlower);
        const bool matchesSearch = search.empty() || searchable.find(search) != std::wstring::npos;
        ShowWindow(checkboxHandles[index], matchesSearch ? SW_SHOW : SW_HIDE);
    }
    LayoutInterface(GetDpiForWindow(window));
}

void ApplyProfile() {
    const int profile = static_cast<int>(SendMessageW(profileBox, CB_GETCURSEL, 0, 0));
    if (profile <= 0) {
        return;
    }
    for (std::size_t index = 0; index < applications.size(); ++index) {
        const bool selected = profile == 1 || (profile >= 2 && applications[index].profile == profile - 1);
        SendMessageW(checkboxHandles[index], BM_SETCHECK, selected ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

void SaveSettings(HWND window) {
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\WindowsAppInstallerGui", 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    DWORD selectedMask = 0;
    for (std::size_t index = 0; index < applications.size(); ++index) {
        if (SendMessageW(checkboxHandles[index], BM_GETCHECK, 0, 0) == BST_CHECKED) selectedMask |= 1u << index;
    }
    const DWORD theme = static_cast<DWORD>(themePreference);
    RECT bounds{};
    GetWindowRect(window, &bounds);
    DWORD values[] = {selectedMask, theme, static_cast<DWORD>(bounds.right - bounds.left), static_cast<DWORD>(bounds.bottom - bounds.top)};
    const wchar_t* names[] = {L"Selection", L"Theme", L"Width", L"Height"};
    for (int i = 0; i < 4; ++i) RegSetValueExW(key, names[i], 0, REG_DWORD, reinterpret_cast<const BYTE*>(&values[i]), sizeof(DWORD));
    RegCloseKey(key);
}

void LoadSettings() {
    DWORD mask = 0, theme = 0, size = sizeof(DWORD);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\WindowsAppInstallerGui", L"Selection", RRF_RT_REG_DWORD, nullptr, &mask, &size);
    size = sizeof(DWORD);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\WindowsAppInstallerGui", L"Theme", RRF_RT_REG_DWORD, nullptr, &theme, &size);
    for (std::size_t index = 0; index < applications.size(); ++index) SendMessageW(checkboxHandles[index], BM_SETCHECK, (mask & (1u << index)) ? BST_CHECKED : BST_UNCHECKED, 0);
    themePreference = static_cast<int>(std::min<DWORD>(theme, 2));
}

void CreateInterface(HWND window) {
    const UINT dpi = GetDpiForWindow(window);
    UpdateFonts(dpi);

    titleLabel = AddLabel(window, L"Windows App Installer Toolkit 2.0", titleFont);
    descriptionLabel = AddLabel(
        window,
        L"Wähle die Anwendungen aus, die über Winget installiert werden sollen.",
        smallFont
    );
    sectionLabel = AddLabel(window, L"ANWENDUNGEN", smallFont);

    searchBox = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, window, reinterpret_cast<HMENU>(ID_SEARCH), GetModuleHandleW(nullptr), nullptr
    );
    SendMessageW(searchBox, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Apps oder Paket-ID suchen …"));
    profileBox = CreateWindowExW(
        0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
        0, 0, 0, 0, window, reinterpret_cast<HMENU>(ID_PROFILE), GetModuleHandleW(nullptr), nullptr
    );
    for (const wchar_t* item : {L"Profil wählen", L"Alles", L"Standard", L"Gaming", L"Entwicklung"}) {
        SendMessageW(profileBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
    }
    SendMessageW(profileBox, CB_SETCURSEL, 0, 0);

    for (std::size_t index = 0; index < applications.size(); ++index) {
        checkboxHandles[index] = CreateWindowExW(
            0,
            L"BUTTON",
            applications[index].name,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            0,
            0,
            0,
            0,
            window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CHECKBOX_FIRST + index)),
            GetModuleHandleW(nullptr),
            nullptr
        );
        SetFont(checkboxHandles[index], normalFont);
    }

    selectAllButton = CreateWindowExW(
        0, L"BUTTON", L"Alle auswählen", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0, 0, 0, 0, window, reinterpret_cast<HMENU>(ID_SELECT_ALL), GetModuleHandleW(nullptr), nullptr
    );
    clearButton = CreateWindowExW(
        0, L"BUTTON", L"Auswahl löschen", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0, 0, 0, 0, window, reinterpret_cast<HMENU>(ID_CLEAR), GetModuleHandleW(nullptr), nullptr
    );
    installButton = CreateWindowExW(
        0, L"BUTTON", L"Auswahl installieren", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        0, 0, 0, 0, window, reinterpret_cast<HMENU>(ID_INSTALL), GetModuleHandleW(nullptr), nullptr
    );
    updateButton = CreateWindowExW(0, L"BUTTON", L"Auswahl aktualisieren", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 0, 0, window, reinterpret_cast<HMENU>(ID_UPDATE), GetModuleHandleW(nullptr), nullptr);
    uninstallButton = CreateWindowExW(0, L"BUTTON", L"Auswahl deinstallieren", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 0, 0, window, reinterpret_cast<HMENU>(ID_UNINSTALL), GetModuleHandleW(nullptr), nullptr);
    cancelButton = CreateWindowExW(0, L"BUTTON", L"Abbrechen", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 0, 0, window, reinterpret_cast<HMENU>(ID_CANCEL), GetModuleHandleW(nullptr), nullptr);
    EnableWindow(cancelButton, FALSE);
    refreshButton = CreateWindowExW(0, L"BUTTON", L"Neu prüfen", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 0, 0, window, reinterpret_cast<HMENU>(ID_REFRESH), GetModuleHandleW(nullptr), nullptr);
    SetFont(selectAllButton, normalFont);
    SetFont(clearButton, normalFont);
    SetFont(installButton, normalFont);
    SetFont(updateButton, normalFont);
    SetFont(uninstallButton, normalFont);
    SetFont(cancelButton, normalFont);

    statusLabel = AddLabel(window, L"Bereit", normalFont);
    hintLabel = AddLabel(
        window,
        L"Statusprüfung, Installation und Updates laufen im Hintergrund.",
        smallFont
    );
    progressBar = CreateWindowExW(0, PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        0, 0, 0, 0, window, nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(progressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    customPackageBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, window, nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(customPackageBox, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Zusätzliche Winget-Paket-ID"));

    for (HWND control : {searchBox, profileBox, refreshButton, customPackageBox}) {
        SetFont(control, smallFont);
    }
    LoadSettings();

    LayoutInterface(dpi);
}

void OpenReleasePage(HWND window) {
    const wchar_t* releaseUrl = L"https://github.com/alimegl/Windows-App-Installer-Toolkit/releases";
    const INT_PTR result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(window, L"open", releaseUrl, nullptr, nullptr, SW_SHOWNORMAL)
    );
    if (result <= 32) {
        MessageBoxW(window, L"Die Release-Seite konnte nicht im Browser geöffnet werden.", L"App-Update", MB_OK | MB_ICONERROR);
    }
}

LRESULT CALLBACK OptionsWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            const UINT dpi = GetDpiForWindow(window);
            HWND label = AddLabel(window, L"Darstellung", normalFont);
            SetControlBounds(label, 24, 22, 340, 28, dpi);

            themeBox = CreateWindowExW(
                0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(ID_THEME), GetModuleHandleW(nullptr), nullptr
            );
            for (const wchar_t* item : {L"Systemdesign", L"Hell", L"Dunkel"}) {
                SendMessageW(themeBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
            }
            SendMessageW(themeBox, CB_SETCURSEL, themePreference, 0);
            SetFont(themeBox, normalFont);
            SetControlBounds(themeBox, 24, 58, 340, 180, dpi);

            HWND applyButton = CreateWindowExW(
                0, L"BUTTON", L"Übernehmen", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(ID_OPTIONS_APPLY), GetModuleHandleW(nullptr), nullptr
            );
            HWND closeButton = CreateWindowExW(
                0, L"BUTTON", L"Schließen", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(ID_OPTIONS_CLOSE), GetModuleHandleW(nullptr), nullptr
            );
            SetFont(applyButton, normalFont);
            SetFont(closeButton, normalFont);
            SetControlBounds(applyButton, 104, 112, 125, 36, dpi);
            SetControlBounds(closeButton, 239, 112, 125, 36, dpi);
            ApplyTheme(window);
            return 0;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_OPTIONS_APPLY) {
                themePreference = static_cast<int>(SendMessageW(themeBox, CB_GETCURSEL, 0, 0));
                HWND owner = GetWindow(window, GW_OWNER);
                if (owner != nullptr) ApplyTheme(owner);
                ApplyTheme(window);
                return 0;
            }
            if (LOWORD(wParam) == ID_OPTIONS_CLOSE) {
                DestroyWindow(window);
                return 0;
            }
            break;

        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT* measureItem = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
            if (measureItem->CtlType == ODT_MENU) {
                MeasureMenuItem(measureItem);
                return TRUE;
            }
            if (measureItem->CtlType == ODT_COMBOBOX) {
                measureItem->itemHeight = static_cast<UINT>(DpiScale(26, GetDpiForWindow(window)));
                return TRUE;
            }
            break;
        }

        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT* drawItem = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (drawItem->CtlType == ODT_MENU) {
                DrawMenuItem(drawItem);
                return TRUE;
            }
            if (drawItem->CtlType == ODT_COMBOBOX) {
                DrawDarkComboItem(drawItem);
                return TRUE;
            }
            break;
        }

        case WM_NOTIFY: {
            const NMHDR* notification = reinterpret_cast<const NMHDR*>(lParam);
            if (notification->code == NM_CUSTOMDRAW) {
                wchar_t className[32]{};
                GetClassNameW(notification->hwndFrom, className, static_cast<int>(std::size(className)));
                if (wcscmp(className, L"Button") == 0) {
                    return DrawDarkButton(reinterpret_cast<const NMCUSTOMDRAW*>(lParam));
                }
            }
            break;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLORLISTBOX: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, textColor);
            SetBkColor(dc, backgroundColor);
            return reinterpret_cast<LRESULT>(backgroundBrush);
        }

        case WM_CLOSE:
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            themeBox = nullptr;
            optionsWindow = nullptr;
            return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void ShowOptionsWindow(HWND owner) {
    if (optionsWindow != nullptr) {
        ShowWindow(optionsWindow, SW_RESTORE);
        SetForegroundWindow(optionsWindow);
        return;
    }

    const wchar_t optionsClass[] = L"WindowsAppInstallerOptions";
    WNDCLASSW definition{};
    definition.lpfnWndProc = OptionsWindowProc;
    definition.hInstance = GetModuleHandleW(nullptr);
    definition.lpszClassName = optionsClass;
    definition.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    definition.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON));
    definition.hbrBackground = backgroundBrush;
    RegisterClassW(&definition);

    const UINT dpi = GetDpiForWindow(owner);
    RECT bounds{0, 0, DpiScale(390, dpi), DpiScale(175, dpi)};
    AdjustWindowRectExForDpi(&bounds, WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME, dpi);
    RECT ownerBounds{};
    GetWindowRect(owner, &ownerBounds);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    optionsWindow = CreateWindowExW(
        WS_EX_DLGMODALFRAME, optionsClass, L"Optionen", WS_CAPTION | WS_SYSMENU,
        ownerBounds.left + (ownerBounds.right - ownerBounds.left - width) / 2,
        ownerBounds.top + (ownerBounds.bottom - ownerBounds.top - height) / 2,
        width, height, owner, nullptr, GetModuleHandleW(nullptr), nullptr
    );
    if (optionsWindow != nullptr) {
        ShowWindow(optionsWindow, SW_SHOW);
        UpdateWindow(optionsWindow);
    }
}

LRESULT CALLBACK InfoWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            const UINT dpi = GetDpiForWindow(window);
            HWND heading = AddLabel(window, L"Windows App Installer Toolkit", titleFont);
            HWND details = AddLabel(
                window,
                L"Native Win32-Oberfläche für Software installationen.\nVersion 2.0 by alimegl.",
                normalFont
            );
            HWND closeButton = CreateWindowExW(
                0, L"BUTTON", L"Schließen", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(ID_INFO_CLOSE), GetModuleHandleW(nullptr), nullptr
            );
            SetFont(closeButton, normalFont);
            SetControlBounds(heading, 24, 22, 430, 42, dpi);
            SetControlBounds(details, 26, 72, 420, 58, dpi);
            SetControlBounds(closeButton, 321, 145, 125, 36, dpi);
            ApplyTheme(window);
            return 0;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_INFO_CLOSE) {
                DestroyWindow(window);
                return 0;
            }
            break;

        case WM_NOTIFY: {
            const NMHDR* notification = reinterpret_cast<const NMHDR*>(lParam);
            if (notification->code == NM_CUSTOMDRAW) {
                wchar_t className[32]{};
                GetClassNameW(notification->hwndFrom, className, static_cast<int>(std::size(className)));
                if (wcscmp(className, L"Button") == 0) {
                    return DrawDarkButton(reinterpret_cast<const NMCUSTOMDRAW*>(lParam));
                }
            }
            break;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, textColor);
            SetBkColor(dc, backgroundColor);
            return reinterpret_cast<LRESULT>(backgroundBrush);
        }

        case WM_CLOSE:
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            infoWindow = nullptr;
            return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void ShowInfoWindow(HWND owner) {
    if (infoWindow != nullptr) {
        ShowWindow(infoWindow, SW_RESTORE);
        SetForegroundWindow(infoWindow);
        return;
    }

    const wchar_t infoClass[] = L"WindowsAppInstallerInfo";
    WNDCLASSW definition{};
    definition.lpfnWndProc = InfoWindowProc;
    definition.hInstance = GetModuleHandleW(nullptr);
    definition.lpszClassName = infoClass;
    definition.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    definition.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON));
    definition.hbrBackground = backgroundBrush;
    RegisterClassW(&definition);

    const UINT dpi = GetDpiForWindow(owner);
    RECT bounds{0, 0, DpiScale(470, dpi), DpiScale(205, dpi)};
    AdjustWindowRectExForDpi(&bounds, WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME, dpi);
    RECT ownerBounds{};
    GetWindowRect(owner, &ownerBounds);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    infoWindow = CreateWindowExW(
        WS_EX_DLGMODALFRAME, infoClass, L"Info", WS_CAPTION | WS_SYSMENU,
        ownerBounds.left + (ownerBounds.right - ownerBounds.left - width) / 2,
        ownerBounds.top + (ownerBounds.bottom - ownerBounds.top - height) / 2,
        width, height, owner, nullptr, GetModuleHandleW(nullptr), nullptr
    );
    if (infoWindow != nullptr) {
        ShowWindow(infoWindow, SW_SHOW);
        UpdateWindow(infoWindow);
    }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            CreateInterface(window);
            ApplyTheme(window);
            StartOperation(window, Operation::Scan);
            return 0;

        case WM_DPICHANGED: {
            const UINT newDpi = HIWORD(wParam);
            const RECT* suggestedBounds = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(
                window,
                nullptr,
                suggestedBounds->left,
                suggestedBounds->top,
                suggestedBounds->right - suggestedBounds->left,
                suggestedBounds->bottom - suggestedBounds->top,
                SWP_NOACTIVATE | SWP_NOZORDER
            );
            UpdateFonts(newDpi);
            LayoutInterface(newDpi);
            return 0;
        }

        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED && titleLabel != nullptr) {
                LayoutInterface(GetDpiForWindow(window));
            }
            return 0;

        case WM_GETMINMAXINFO: {
            MINMAXINFO* sizeInfo = reinterpret_cast<MINMAXINFO*>(lParam);
            const UINT dpi = GetDpiForWindow(window);
            RECT minimumBounds{
                0,
                0,
                DpiScale(MIN_CLIENT_WIDTH, dpi),
                DpiScale(MIN_CLIENT_HEIGHT, dpi)
            };
            const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
            AdjustWindowRectExForDpi(&minimumBounds, style, FALSE, 0, dpi);
            sizeInfo->ptMinTrackSize.x = minimumBounds.right - minimumBounds.left;
            sizeInfo->ptMinTrackSize.y = minimumBounds.bottom - minimumBounds.top;
            return 0;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_SEARCH && HIWORD(wParam) == EN_CHANGE) {
                ApplyFilters(window);
                return 0;
            }
            if (LOWORD(wParam) == ID_PROFILE && HIWORD(wParam) == CBN_SELCHANGE) {
                ApplyProfile();
                return 0;
            }
            switch (LOWORD(wParam)) {
                case ID_MENU_OPTIONS:
                    ShowOptionsWindow(window);
                    return 0;
                case ID_MENU_APP_UPDATE:
                    OpenReleasePage(window);
                    return 0;
                case ID_MENU_INFO:
                    ShowInfoWindow(window);
                    return 0;
                case ID_SELECT_ALL:
                    SelectAll(true);
                    return 0;
                case ID_CLEAR:
                    SelectAll(false);
                    return 0;
                case ID_INSTALL:
                    StartOperation(window, Operation::Install);
                    return 0;
                case ID_UPDATE:
                    StartOperation(window, Operation::Update);
                    return 0;
                case ID_UNINSTALL:
                    if (MessageBoxW(window, L"Die ausgewählten Anwendungen wirklich deinstallieren?", L"Deinstallation bestätigen", MB_YESNO | MB_ICONWARNING) == IDYES) {
                        StartOperation(window, Operation::Uninstall);
                    }
                    return 0;
                case ID_CANCEL: {
                    cancellationRequested = true;
                    HANDLE process = runningProcess.load();
                    if (process != nullptr) TerminateProcess(process, ERROR_CANCELLED);
                    SetWindowTextW(statusLabel, L"Auftrag wird abgebrochen …");
                    return 0;
                }
                case ID_REFRESH:
                    StartOperation(window, Operation::Scan);
                    return 0;
            }
            break;

        case WM_APP_STATUS: {
            std::wstring* status = reinterpret_cast<std::wstring*>(lParam);
            SetWindowTextW(statusLabel, status->c_str());
            delete status;
            return 0;
        }

        case WM_APP_PROGRESS:
            SendMessageW(progressBar, PBM_SETPOS, lParam == 0 ? 0 : static_cast<int>(wParam * 100 / lParam), 0);
            return 0;

        case WM_APP_SCAN_FINISHED:
            installationRunning = false;
            SetInstallerControlsEnabled(true);
            UpdateApplicationLabels();
            SetWindowTextW(statusLabel, L"Prüfung abgeschlossen. Installierte Apps und verfügbare Updates sind markiert.");
            SendMessageW(progressBar, PBM_SETPOS, 100, 0);
            return 0;

        case WM_APP_FINISHED: {
            const int successful = static_cast<int>(wParam);
            const int total = static_cast<int>(lParam);
            installationRunning = false;
            SetInstallerControlsEnabled(true);
            SendMessageW(progressBar, PBM_SETPOS, 100, 0);

            std::wstring result = std::to_wstring(successful) + L" von " + std::to_wstring(total);
            result += cancellationRequested.load() ? L" Vorgängen vor dem Abbruch erfolgreich." : L" Vorgängen erfolgreich.";
            if (restartRequired.load()) result += L" Ein Windows-Neustart wird empfohlen.";
            SetWindowTextW(statusLabel, result.c_str());

            MessageBoxW(
                window,
                result.c_str(),
                cancellationRequested.load() ? L"Auftrag abgebrochen" : (successful == total ? L"Auftrag abgeschlossen" : L"Auftrag mit Fehlern abgeschlossen"),
                MB_OK | (successful == total ? MB_ICONINFORMATION : MB_ICONWARNING)
            );
            return 0;
        }

        case WM_SETTINGCHANGE:
            ApplyTheme(window);
            return 0;

        case WM_NOTIFY: {
            const NMHDR* notification = reinterpret_cast<const NMHDR*>(lParam);
            if (notification->code == NM_CUSTOMDRAW) {
                wchar_t className[32]{};
                GetClassNameW(notification->hwndFrom, className, static_cast<int>(std::size(className)));
                if (wcscmp(className, L"Button") == 0) {
                    return DrawDarkButton(reinterpret_cast<const NMCUSTOMDRAW*>(lParam));
                }
            }
            break;
        }

        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT* measureItem = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
            if (measureItem->CtlType == ODT_MENU) {
                MeasureMenuItem(measureItem);
                return TRUE;
            }
            if (measureItem->CtlType == ODT_COMBOBOX) {
                measureItem->itemHeight = static_cast<UINT>(DpiScale(26, GetDpiForWindow(window)));
                return TRUE;
            }
            break;
        }

        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT* drawItem = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (drawItem->CtlType == ODT_MENU) {
                DrawMenuItem(drawItem);
                return TRUE;
            }
            if (drawItem->CtlType == ODT_COMBOBOX) {
                DrawDarkComboItem(drawItem);
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC deviceContext = reinterpret_cast<HDC>(wParam);
            HWND control = reinterpret_cast<HWND>(lParam);
            const bool usesMutedText = control == descriptionLabel || control == sectionLabel || control == hintLabel;
            SetTextColor(deviceContext, usesMutedText ? mutedTextColor : textColor);
            SetBkColor(deviceContext, backgroundColor);
            return reinterpret_cast<LRESULT>(backgroundBrush);
        }

        case WM_CLOSE:
            if (installationRunning) {
                if (MessageBoxW(window, L"Der laufende Auftrag wird abgebrochen. Fenster wirklich schließen?", L"Auftrag läuft", MB_YESNO | MB_ICONWARNING) != IDYES) return 0;
                cancellationRequested = true;
                HANDLE process = runningProcess.load();
                if (process != nullptr) TerminateProcess(process, ERROR_CANCELLED);
            }
            SaveSettings(window);
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            DeleteObject(titleFont);
            DeleteObject(normalFont);
            DeleteObject(smallFont);
            DeleteObject(menuFont);
            DeleteObject(backgroundBrush);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int RunWindowsAppInstaller(HINSTANCE instance, int showCommand) {
    const wchar_t windowClass[] = L"WindowsAppInstallerGui";
    constexpr DWORD windowStyle = WS_OVERLAPPEDWINDOW;

    SetNativeAppMode(PreferredAppMode::AllowDark);
    darkModeEnabled = IsWindowsDarkModeEnabled();
    UpdateThemeColors(darkModeEnabled);
    backgroundBrush = CreateSolidBrush(backgroundColor);

    WNDCLASSW windowDefinition{};
    windowDefinition.lpfnWndProc = WindowProc;
    windowDefinition.hInstance = instance;
    windowDefinition.lpszClassName = windowClass;
    windowDefinition.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowDefinition.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    windowDefinition.hbrBackground = backgroundBrush;

    if (!RegisterClassW(&windowDefinition)) {
        DeleteObject(backgroundBrush);
        return 1;
    }

    NONCLIENTMETRICSW nonClientMetrics{};
    nonClientMetrics.cbSize = sizeof(nonClientMetrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(nonClientMetrics), &nonClientMetrics, 0)) {
        menuFont = CreateFontIndirectW(&nonClientMetrics.lfMenuFont);
    }

    mainMenuBar = CreateMenu();
    optionsPopupMenu = CreatePopupMenu();
    helpPopupMenu = CreatePopupMenu();
    AppendMenuW(optionsPopupMenu, MF_STRING, ID_MENU_OPTIONS, L"Optionen …");
    AppendMenuW(helpPopupMenu, MF_STRING, ID_MENU_APP_UPDATE, L"App-Update");
    AppendMenuW(helpPopupMenu, MF_STRING, ID_MENU_INFO, L"Info");
    AppendMenuW(mainMenuBar, MF_POPUP | MF_OWNERDRAW, reinterpret_cast<UINT_PTR>(optionsPopupMenu), reinterpret_cast<LPCWSTR>(L"Optionen"));
    AppendMenuW(mainMenuBar, MF_POPUP | MF_OWNERDRAW, reinterpret_cast<UINT_PTR>(helpPopupMenu), reinterpret_cast<LPCWSTR>(L"Hilfe"));

    const UINT initialDpi = GetDpiForSystem();
    RECT windowBounds{
        0,
        0,
        DpiScale(BASE_CLIENT_WIDTH, initialDpi),
        DpiScale(BASE_CLIENT_HEIGHT, initialDpi)
    };
    AdjustWindowRectExForDpi(&windowBounds, windowStyle, TRUE, 0, initialDpi);

    int windowWidth = windowBounds.right - windowBounds.left;
    int windowHeight = windowBounds.bottom - windowBounds.top;
    DWORD savedWidth = 0;
    DWORD savedHeight = 0;
    DWORD savedSize = sizeof(DWORD);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\WindowsAppInstallerGui", L"Width", RRF_RT_REG_DWORD, nullptr, &savedWidth, &savedSize);
    savedSize = sizeof(DWORD);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\WindowsAppInstallerGui", L"Height", RRF_RT_REG_DWORD, nullptr, &savedHeight, &savedSize);
    POINT primaryMonitorPoint{0, 0};
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfoW(MonitorFromPoint(primaryMonitorPoint, MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
    const int workWidth = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
    const int workHeight = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
    if (savedWidth >= static_cast<DWORD>(DpiScale(MIN_CLIENT_WIDTH, initialDpi))) windowWidth = std::min(static_cast<int>(savedWidth), workWidth);
    if (savedHeight >= static_cast<DWORD>(DpiScale(MIN_CLIENT_HEIGHT, initialDpi))) windowHeight = std::min(static_cast<int>(savedHeight), workHeight);
    const int windowX = monitorInfo.rcWork.left + (monitorInfo.rcWork.right - monitorInfo.rcWork.left - windowWidth) / 2;
    const int windowY = monitorInfo.rcWork.top + (monitorInfo.rcWork.bottom - monitorInfo.rcWork.top - windowHeight) / 2;

    HWND window = CreateWindowExW(
        0,
        windowClass,
        L"Windows App Installer Toolkit 2.0",
        windowStyle,
        windowX,
        windowY,
        windowWidth,
        windowHeight,
        nullptr,
        mainMenuBar,
        instance,
        nullptr
    );

    if (window == nullptr) {
        DestroyMenu(mainMenuBar);
        DeleteObject(menuFont);
        DeleteObject(backgroundBrush);
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
