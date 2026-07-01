#include "winget_service.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cwctype>
#include <utility>

namespace {

using winget_service::Operation;

struct Job {
    HWND window;
    std::vector<int> selectedApplications;
    Operation operation;
    std::wstring customPackage;
};

std::atomic_bool running{false};
std::atomic_bool cancellationRequested{false};
std::atomic_bool restartRequired{false};
std::atomic<HANDLE> runningProcess{nullptr};
std::array<bool, applications.size()> installedApplications{};
std::array<bool, applications.size()> updateableApplications{};

void PostStatus(HWND window, std::wstring text) {
    PostMessageW(
        window,
        winget_service::STATUS_MESSAGE,
        0,
        reinterpret_cast<LPARAM>(new std::wstring(std::move(text)))
    );
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

bool RunCommand(const std::wstring& arguments, DWORD& exitCode, std::wstring* capturedOutput = nullptr) {
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
        L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &securityAttributes,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr
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
        nullptr, writableCommand.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
        nullptr, nullptr, &startupInfo, &processInfo
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
        if (capturedOutput != nullptr) {
            *capturedOutput += DecodeConsoleOutput(outputBuffer.data(), static_cast<int>(bytesRead));
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

DWORD WINAPI Worker(void* parameter) {
    Job* job = static_cast<Job*>(parameter);
    DWORD exitCode = 0;
    if (job->operation == Operation::Scan) {
        std::wstring listed;
        std::wstring upgrades;
        PostStatus(job->window, L"Installierte Apps und Updates werden geprüft …");
        RunCommand(L"list --accept-source-agreements --disable-interactivity", exitCode, &listed);
        RunCommand(L"upgrade --accept-source-agreements --disable-interactivity", exitCode, &upgrades);
        std::transform(listed.begin(), listed.end(), listed.begin(), towlower);
        std::transform(upgrades.begin(), upgrades.end(), upgrades.begin(), towlower);
        for (std::size_t index = 0; index < applications.size(); ++index) {
            std::wstring id = applications[index].packageId;
            std::transform(id.begin(), id.end(), id.begin(), towlower);
            installedApplications[index] = listed.find(id) != std::wstring::npos;
            updateableApplications[index] = upgrades.find(id) != std::wstring::npos;
        }
        running = false;
        PostMessageW(job->window, winget_service::SCAN_FINISHED_MESSAGE, 0, 0);
        delete job;
        return 0;
    }

    if (job->operation == Operation::Install) {
        PostStatus(job->window, L"Winget-Paketquelle wird aktualisiert …");
        RunCommand(L"source update --name winget --disable-interactivity", exitCode);
    }

    int successful = 0;
    const int total = static_cast<int>(job->selectedApplications.size()) + (job->customPackage.empty() ? 0 : 1);
    int current = 0;
    auto runPackage = [&](const std::wstring& name, const std::wstring& id, const std::wstring& source) {
        if (cancellationRequested.load()) {
            return;
        }
        ++current;
        PostMessageW(job->window, winget_service::PROGRESS_MESSAGE, current, total);
        const wchar_t* verb = job->operation == Operation::Install ? L"Installiere "
            : job->operation == Operation::Update ? L"Aktualisiere " : L"Deinstalliere ";
        PostStatus(job->window, verb + name + L" …");
        std::wstring arguments = job->operation == Operation::Install ? L"install"
            : job->operation == Operation::Update ? L"upgrade" : L"uninstall";
        arguments += L" -e --id \"" + id + L"\"";
        if (!source.empty() && job->operation != Operation::Uninstall) {
            arguments += L" --source \"" + source + L"\"";
        }
        arguments += L" --accept-source-agreements --disable-interactivity";
        if (job->operation != Operation::Uninstall) {
            arguments += L" --accept-package-agreements";
        }
        if (RunCommand(arguments, exitCode)) {
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

    running = false;
    PostMessageW(job->window, winget_service::FINISHED_MESSAGE, successful, total);
    delete job;
    return 0;
}

} // namespace

namespace winget_service {

bool IsAvailable() {
    return SearchPathW(nullptr, L"winget.exe", nullptr, 0, nullptr, nullptr) > 0;
}

bool IsRunning() { return running.load(); }
bool WasCancelled() { return cancellationRequested.load(); }
bool RestartRequired() { return restartRequired.load(); }
const std::array<bool, applications.size()>& InstalledApplications() { return installedApplications; }
const std::array<bool, applications.size()>& UpdateableApplications() { return updateableApplications; }

bool Start(HWND window, Operation operation, std::vector<int> selectedApplications, std::wstring customPackage) {
    if (running.exchange(true)) {
        return false;
    }
    cancellationRequested = false;
    restartRequired = false;
    Job* job = new Job{window, std::move(selectedApplications), operation, std::move(customPackage)};
    HANDLE thread = CreateThread(nullptr, 0, Worker, job, 0, nullptr);
    if (thread == nullptr) {
        delete job;
        running = false;
        return false;
    }
    CloseHandle(thread);
    return true;
}

void Cancel() {
    cancellationRequested = true;
    HANDLE process = runningProcess.load();
    if (process != nullptr) {
        TerminateProcess(process, ERROR_CANCELLED);
    }
}

} // namespace winget_service
