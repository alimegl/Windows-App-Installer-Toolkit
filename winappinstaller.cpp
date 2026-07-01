#include "winappinstaller.h"

#include <dwmapi.h>
#include <uxtheme.h>

#include <array>
#include <atomic>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Application {
    const wchar_t* name;
    const wchar_t* packageId;
    const wchar_t* source;
};

constexpr std::array<Application, 13> applications{{
    {L"Discord", L"Discord.Discord", L"winget"},
    {L"WhatsApp", L"9NKSQGP7F2NH", L"msstore"},
    {L"Apple Music", L"9PFHDD62MXS1", L"msstore"},
    {L"Streamlabs OBS", L"Streamlabs.Streamlabs", L"winget"},
    {L"Steam", L"Valve.Steam", L"winget"},
    {L"Ubisoft Connect", L"Ubisoft.Connect", L"winget"},
    {L"EA App", L"ElectronicArts.EADesktop", L"winget"},
    {L"Epic Games Launcher", L"EpicGames.EpicGamesLauncher", L"winget"},
    {L"Rockstar Games Launcher", L"RockstarGames.Launcher", L"winget"},
    {L"Visual Studio Code", L"Microsoft.VisualStudioCode", L"winget"},
    {L"Google Chrome", L"Google.Chrome", L"winget"},
    {L"MSYS2", L"MSYS2.MSYS2", L"winget"},
    {L"Git", L"Git.Git", L"winget"},
}};

constexpr int ID_CHECKBOX_FIRST = 1000;
constexpr int ID_SELECT_ALL = 2000;
constexpr int ID_CLEAR = 2001;
constexpr int ID_INSTALL = 2002;
constexpr UINT WM_APP_STATUS = WM_APP + 1;
constexpr UINT WM_APP_FINISHED = WM_APP + 2;
constexpr UINT WM_APP_OUTPUT = WM_APP + 3;

constexpr COLORREF LIGHT_BACKGROUND_COLOR = RGB(245, 247, 250);
constexpr COLORREF LIGHT_TEXT_COLOR = RGB(28, 35, 48);
constexpr COLORREF LIGHT_MUTED_COLOR = RGB(91, 101, 117);
constexpr COLORREF DARK_BACKGROUND_COLOR = RGB(32, 32, 32);
constexpr COLORREF DARK_TEXT_COLOR = RGB(243, 243, 243);
constexpr COLORREF DARK_MUTED_COLOR = RGB(184, 188, 194);
constexpr int BASE_CLIENT_WIDTH = 820;
constexpr int BASE_CLIENT_HEIGHT = 810;
constexpr int MIN_CLIENT_WIDTH = 638;
constexpr int MIN_CLIENT_HEIGHT = 700;

std::array<HWND, applications.size()> checkboxHandles{};
HWND titleLabel = nullptr;
HWND descriptionLabel = nullptr;
HWND sectionLabel = nullptr;
HWND hintLabel = nullptr;
HWND consoleLabel = nullptr;
HWND outputBox = nullptr;
HWND installButton = nullptr;
HWND selectAllButton = nullptr;
HWND clearButton = nullptr;
HWND statusLabel = nullptr;
HFONT titleFont = nullptr;
HFONT normalFont = nullptr;
HFONT smallFont = nullptr;
HFONT consoleFont = nullptr;
HBRUSH backgroundBrush = nullptr;
std::atomic_bool installationRunning{false};
bool darkModeEnabled = false;
COLORREF backgroundColor = LIGHT_BACKGROUND_COLOR;
COLORREF textColor = LIGHT_TEXT_COLOR;
COLORREF mutedTextColor = LIGHT_MUTED_COLOR;

struct InstallJob {
    HWND window;
    std::vector<int> selectedApplications;
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
    HFONT oldConsoleFont = consoleFont;

    titleFont = CreateUiFont(28, FW_SEMIBOLD, dpi);
    normalFont = CreateUiFont(17, FW_NORMAL, dpi);
    smallFont = CreateUiFont(15, FW_NORMAL, dpi);
    consoleFont = CreateFontW(
        -DpiScale(15, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, L"Consolas"
    );

    if (titleLabel != nullptr) {
        SetFont(titleLabel, titleFont);
        SetFont(descriptionLabel, smallFont);
        SetFont(sectionLabel, smallFont);
        SetFont(hintLabel, smallFont);
        SetFont(consoleLabel, smallFont);
        SetFont(outputBox, consoleFont);
        SetFont(statusLabel, normalFont);
        SetFont(selectAllButton, normalFont);
        SetFont(clearButton, normalFont);
        SetFont(installButton, normalFont);
        for (HWND checkbox : checkboxHandles) {
            SetFont(checkbox, normalFont);
        }
    }

    DeleteObject(oldTitleFont);
    DeleteObject(oldNormalFont);
    DeleteObject(oldSmallFont);
    DeleteObject(oldConsoleFont);
}

void LayoutInterface(UINT dpi) {
    RECT clientBounds{};
    GetClientRect(GetParent(titleLabel), &clientBounds);
    const int clientWidth = MulDiv(clientBounds.right, USER_DEFAULT_SCREEN_DPI, static_cast<int>(dpi));
    const int clientHeight = MulDiv(clientBounds.bottom, USER_DEFAULT_SCREEN_DPI, static_cast<int>(dpi));
    const int contentWidth = clientWidth - 68;
    const int columnWidth = (contentWidth - 25) / 2;

    SetControlBounds(titleLabel, 32, 24, contentWidth, 40, dpi);
    SetControlBounds(descriptionLabel, 34, 67, contentWidth, 25, dpi);
    SetControlBounds(sectionLabel, 34, 110, 200, 22, dpi);

    for (std::size_t index = 0; index < applications.size(); ++index) {
        const int column = index < 7 ? 0 : 1;
        const int row = column == 0 ? static_cast<int>(index) : static_cast<int>(index) - 7;
        SetControlBounds(
            checkboxHandles[index],
            34 + column * (columnWidth + 25),
            140 + row * 42,
            columnWidth,
            28,
            dpi
        );
    }

    SetControlBounds(selectAllButton, 34, 449, 145, 36, dpi);
    SetControlBounds(clearButton, 190, 449, 145, 36, dpi);
    SetControlBounds(installButton, clientWidth - 236, 449, 202, 36, dpi);
    SetControlBounds(statusLabel, 34, 510, contentWidth, 28, dpi);
    SetControlBounds(hintLabel, 34, 542, contentWidth, 22, dpi);
    SetControlBounds(consoleLabel, 34, 578, contentWidth, 22, dpi);
    SetControlBounds(outputBox, 34, 606, contentWidth, clientHeight - 636, dpi);
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
    darkModeEnabled = IsWindowsDarkModeEnabled();
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
    for (HWND checkbox : checkboxHandles) {
        SetWindowTheme(checkbox, controlTheme, nullptr);
    }
    SetWindowTheme(selectAllButton, controlTheme, nullptr);
    SetWindowTheme(clearButton, controlTheme, nullptr);
    SetWindowTheme(installButton, controlTheme, nullptr);
    SetWindowTheme(outputBox, controlTheme, nullptr);

    RedrawWindow(window, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void PostStatus(HWND window, std::wstring text) {
    PostMessageW(window, WM_APP_STATUS, 0, reinterpret_cast<LPARAM>(new std::wstring(std::move(text))));
}

void PostOutput(HWND window, std::wstring text) {
    PostMessageW(window, WM_APP_OUTPUT, 0, reinterpret_cast<LPARAM>(new std::wstring(std::move(text))));
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

bool RunCommand(HWND window, const std::wstring& arguments, DWORD& exitCode) {
    std::wstring commandLine = L"winget.exe " + arguments;
    std::vector<wchar_t> writableCommand(commandLine.begin(), commandLine.end());
    writableCommand.push_back(L'\0');

    PostOutput(window, L"\r\n> " + commandLine + L"\r\n");

    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &securityAttributes, 0)) {
        exitCode = GetLastError();
        PostOutput(window, L"[Pipe-Fehler: " + std::to_wstring(exitCode) + L"]\r\n");
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
        PostOutput(window, L"[Prozessfehler: " + std::to_wstring(exitCode) + L"]\r\n");
        return false;
    }

    CloseHandle(processInfo.hThread);

    std::array<char, 4096> outputBuffer{};
    DWORD bytesRead = 0;
    while (ReadFile(readPipe, outputBuffer.data(), static_cast<DWORD>(outputBuffer.size()), &bytesRead, nullptr)
           && bytesRead > 0) {
        PostOutput(window, DecodeConsoleOutput(outputBuffer.data(), static_cast<int>(bytesRead)));
    }
    CloseHandle(readPipe);

    WaitForSingleObject(processInfo.hProcess, INFINITE);
    const bool receivedExitCode = GetExitCodeProcess(processInfo.hProcess, &exitCode) != FALSE;
    CloseHandle(processInfo.hProcess);

    PostOutput(window, L"\r\n[Exitcode: " + std::to_wstring(exitCode) + L"]\r\n");
    return receivedExitCode && exitCode == 0;
}

DWORD WINAPI InstallWorker(void* parameter) {
    InstallJob* job = static_cast<InstallJob*>(parameter);
    DWORD exitCode = 0;

    PostStatus(job->window, L"Winget-Paketquelle wird aktualisiert ...");
    RunCommand(job->window, L"source update --name winget --disable-interactivity", exitCode);

    int successful = 0;
    for (int index : job->selectedApplications) {
        const Application& application = applications[static_cast<std::size_t>(index)];
        PostStatus(job->window, std::wstring(L"Installiere ") + application.name + L" ...");

        std::wstring arguments = L"install -e --id \"";
        arguments += application.packageId;
        arguments += L"\" --source \"";
        arguments += application.source;
        arguments += L"\" --accept-package-agreements --accept-source-agreements --disable-interactivity";

        if (RunCommand(job->window, arguments, exitCode)) {
            ++successful;
        }
    }

    PostMessageW(
        job->window,
        WM_APP_FINISHED,
        static_cast<WPARAM>(successful),
        static_cast<LPARAM>(job->selectedApplications.size())
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
}

void SelectAll(bool selected) {
    for (HWND checkbox : checkboxHandles) {
        SendMessageW(checkbox, BM_SETCHECK, selected ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

void StartInstallation(HWND window) {
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

    if (selected.empty()) {
        MessageBoxW(window, L"Bitte wähle mindestens eine Anwendung aus.", L"Keine Auswahl", MB_OK | MB_ICONINFORMATION);
        return;
    }

    installationRunning = true;
    SetInstallerControlsEnabled(false);
    SetWindowTextW(statusLabel, L"Installation wird vorbereitet ...");
    SetWindowTextW(outputBox, L"Windows App Installer – Winget-Ausgabe\r\n");

    InstallJob* job = new InstallJob{window, std::move(selected)};
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

void CreateInterface(HWND window) {
    const UINT dpi = GetDpiForWindow(window);
    UpdateFonts(dpi);

    titleLabel = AddLabel(window, L"Windows App Installer", titleFont);
    descriptionLabel = AddLabel(
        window,
        L"Wähle die Anwendungen aus, die über Winget installiert werden sollen.",
        smallFont
    );
    sectionLabel = AddLabel(window, L"ANWENDUNGEN", smallFont);

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

    SetFont(selectAllButton, normalFont);
    SetFont(clearButton, normalFont);
    SetFont(installButton, normalFont);

    statusLabel = AddLabel(window, L"Bereit", normalFont);
    hintLabel = AddLabel(
        window,
        L"Während der Installation können Windows- oder Administratorabfragen erscheinen.",
        smallFont
    );
    consoleLabel = AddLabel(window, L"WINGET-AUSGABE", smallFont);
    outputBox = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"Bereit. Wähle Anwendungen aus und starte die Installation.\r\n",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_NOHIDESEL,
        0,
        0,
        0,
        0,
        window,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );
    SendMessageW(outputBox, EM_SETLIMITTEXT, 1024 * 1024, 0);
    SetFont(outputBox, consoleFont);

    LayoutInterface(dpi);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            CreateInterface(window);
            ApplyTheme(window);
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
            switch (LOWORD(wParam)) {
                case ID_SELECT_ALL:
                    SelectAll(true);
                    return 0;
                case ID_CLEAR:
                    SelectAll(false);
                    return 0;
                case ID_INSTALL:
                    StartInstallation(window);
                    return 0;
            }
            break;

        case WM_APP_STATUS: {
            std::wstring* status = reinterpret_cast<std::wstring*>(lParam);
            SetWindowTextW(statusLabel, status->c_str());
            delete status;
            return 0;
        }

        case WM_APP_OUTPUT: {
            std::wstring* output = reinterpret_cast<std::wstring*>(lParam);
            const int textLength = GetWindowTextLengthW(outputBox);
            SendMessageW(outputBox, EM_SETSEL, textLength, textLength);
            SendMessageW(outputBox, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(output->c_str()));
            SendMessageW(outputBox, EM_SCROLLCARET, 0, 0);
            delete output;
            return 0;
        }

        case WM_APP_FINISHED: {
            const int successful = static_cast<int>(wParam);
            const int total = static_cast<int>(lParam);
            installationRunning = false;
            SetInstallerControlsEnabled(true);

            std::wstring result = std::to_wstring(successful) + L" von " + std::to_wstring(total);
            result += L" Installationen erfolgreich.";
            SetWindowTextW(statusLabel, result.c_str());

            MessageBoxW(
                window,
                result.c_str(),
                successful == total ? L"Installation abgeschlossen" : L"Installation mit Fehlern abgeschlossen",
                MB_OK | (successful == total ? MB_ICONINFORMATION : MB_ICONWARNING)
            );
            return 0;
        }

        case WM_SETTINGCHANGE:
            ApplyTheme(window);
            return 0;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT: {
            HDC deviceContext = reinterpret_cast<HDC>(wParam);
            HWND control = reinterpret_cast<HWND>(lParam);
            const bool usesMutedText = control == descriptionLabel || control == sectionLabel
                || control == hintLabel || control == consoleLabel;
            SetTextColor(deviceContext, usesMutedText ? mutedTextColor : textColor);
            SetBkColor(deviceContext, backgroundColor);
            return reinterpret_cast<LRESULT>(backgroundBrush);
        }

        case WM_CLOSE:
            if (installationRunning) {
                MessageBoxW(
                    window,
                    L"Bitte warte, bis die laufende Installation abgeschlossen ist.",
                    L"Installation läuft",
                    MB_OK | MB_ICONINFORMATION
                );
                return 0;
            }
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            DeleteObject(titleFont);
            DeleteObject(normalFont);
            DeleteObject(smallFont);
            DeleteObject(consoleFont);
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

    darkModeEnabled = IsWindowsDarkModeEnabled();
    UpdateThemeColors(darkModeEnabled);
    backgroundBrush = CreateSolidBrush(backgroundColor);

    WNDCLASSW windowDefinition{};
    windowDefinition.lpfnWndProc = WindowProc;
    windowDefinition.hInstance = instance;
    windowDefinition.lpszClassName = windowClass;
    windowDefinition.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowDefinition.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowDefinition.hbrBackground = backgroundBrush;

    if (!RegisterClassW(&windowDefinition)) {
        DeleteObject(backgroundBrush);
        return 1;
    }

    const UINT initialDpi = GetDpiForSystem();
    RECT windowBounds{
        0,
        0,
        DpiScale(BASE_CLIENT_WIDTH, initialDpi),
        DpiScale(BASE_CLIENT_HEIGHT, initialDpi)
    };
    AdjustWindowRectExForDpi(&windowBounds, windowStyle, FALSE, 0, initialDpi);

    const int windowWidth = windowBounds.right - windowBounds.left;
    const int windowHeight = windowBounds.bottom - windowBounds.top;
    POINT primaryMonitorPoint{0, 0};
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfoW(MonitorFromPoint(primaryMonitorPoint, MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
    const int windowX = monitorInfo.rcWork.left + (monitorInfo.rcWork.right - monitorInfo.rcWork.left - windowWidth) / 2;
    const int windowY = monitorInfo.rcWork.top + (monitorInfo.rcWork.bottom - monitorInfo.rcWork.top - windowHeight) / 2;

    HWND window = CreateWindowExW(
        0,
        windowClass,
        L"Windows App Installer",
        windowStyle,
        windowX,
        windowY,
        windowWidth,
        windowHeight,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (window == nullptr) {
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
