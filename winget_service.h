#ifndef WINGET_SERVICE_H
#define WINGET_SERVICE_H

#include "applications.h"

#include <windows.h>

#include <array>
#include <string>
#include <vector>

namespace winget_service {

enum class Operation { Scan, Install, Update, Uninstall };

inline constexpr UINT STATUS_MESSAGE = WM_APP + 1;
inline constexpr UINT FINISHED_MESSAGE = WM_APP + 2;
inline constexpr UINT SCAN_FINISHED_MESSAGE = WM_APP + 3;
inline constexpr UINT PROGRESS_MESSAGE = WM_APP + 4;

bool IsAvailable();
bool IsRunning();
bool WasCancelled();
bool RestartRequired();
const std::array<bool, applications.size()>& InstalledApplications();
const std::array<bool, applications.size()>& UpdateableApplications();

bool Start(HWND window, Operation operation, std::vector<int> selectedApplications, std::wstring customPackage);
void Cancel();

} // namespace winget_service

#endif
