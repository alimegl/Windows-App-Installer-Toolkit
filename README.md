# Windows App Installer Toolkit

A small C++ command-line application that installs commonly used Windows applications through the Windows Package Manager (`winget`). You can install either every listed application or one application selected by number.

## Supported applications right now

1. Discord
2. WhatsApp
3. Apple Music
4. Streamlabs OBS
5. Steam
6. Ubisoft Connect
7. EA App
8. Epic Games Launcher
9. Rockstar Games Launcher
10. Visual Studio Code
11. Google Chrome
12. MSYS2
13. Git

## Requirements for building from Source

- Windows 10 version 1809 or later (absolutely needed, because since then WinGet is included)
- An internet connection
- A C++17-compatible compiler

You can check whether Winget is available with:

```powershell
winget --version
```

## Build

Open PowerShell in the project directory and compile both source files:

```powershell
g++ main.cpp winappinstaller.cpp -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -o WindowsAppInstaller.exe
```

## Run

```powershell
.\winappinstaller.exe
```

## Usage

1. Select `1` in the main menu to open the application list.
2. Enter `y` to install every listed application.
3. Enter `n` to install a single application, then enter its number from `1` to `13`.
4. Select `2` in the main menu to exit.

> **Warning:** Choosing `y` immediately starts the installation of every listed application. Winget or Windows may display confirmation or administrator prompts during installation.



## Notes

- Before starting an installation, the tool refreshes the `winget` community source.
- Most applications are installed from the `winget` community source using their package IDs.
- WhatsApp and Apple Music are installed from the Microsoft Store (`msstore`) source.
- The installer runs Winget commands sequentially.
- If an installation fails, review the message printed by Winget in the terminal.
