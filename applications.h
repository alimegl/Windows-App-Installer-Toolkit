#ifndef APPLICATIONS_H
#define APPLICATIONS_H

#include <array>

struct Application {
    const wchar_t* name;
    const wchar_t* packageId;
    const wchar_t* source;
    int profile;
};

inline constexpr std::array<Application, 13> applications{{
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

#endif
