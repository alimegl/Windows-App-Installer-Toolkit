# Windows App Installer

<p align="center">
  <img src="winapptoolkit.ico" alt="Windows App Installer Toolkit icon" width="128" height="128">
</p>

A native Windows GUI for managing commonly used applications through the Windows Package Manager (`winget`).

## Features

- detection of installed applications and available updates
- install, update, uninstall, and cancel actions
- application search and automatically applied selection profiles
- overall progress and compact status display without a console area
- custom Winget package IDs
- automatic, light, and dark theme modes
- native **Options** and **Help** menus with a separate settings window
- direct access to the GitHub releases page for application updates
- saved selection, options, and window size
- DPI scaling for different display sizes
- standalone, statically linked executable

## Supported applications

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

## Runtime requirements

No compiler is required to run the compiled `WindowsAppInstaller.exe`. The C/C++ runtime is linked statically, so Visual Studio, MSYS2, MinGW, and their runtime DLLs do not need to be installed on the target computer.

Only the following are required:

- 64-bit Windows 10 or Windows 11
- an internet connection
- a working `winget` installation

Check whether Winget is available from PowerShell:

```powershell
winget --version
```

If the command is unavailable, install or update **App Installer** from the Microsoft Store.

## Building

MSYS2 C++ compiler and CMake are only required on the computer used to build the project. The resulting executable can then run on other compatible Windows computers without a compiler or compiler runtime.

### Build Tools

Requirements: MSYS2 with G++ compiler.
Install MSYS2, open ucrt64 and run the following command:
```pacman -S --needed \
mingw-w64-ucrt-x86_64-gcc \
mingw-w64-ucrt-x86_64-cmake \
mingw-w64-ucrt-x86_64-ninja
```

```powershell
cmake -S . -B build -G Ninja `
-DCMAKE_BUILD_TYPE=Release `
-DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe `
-DCMAKE_RC_COMPILER=C:/msys64/ucrt64/bin/windres.exe

cmake --build build --parallel
cmake --install build --prefix dist
```

Output:

```text
build\WindowsAppInstaller.exe
```

### MSYS2 / MinGW-w64

Run the following commands from an UCRT64 shell:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Output:

```text
build/WindowsAppInstaller.exe
```

CMake uses the static runtime (`/MT`) with MSVC and static linking (`-static`, `-static-libgcc`, and `-static-libstdc++`) with MinGW. The resulting executable therefore does not require a compiler runtime on the target computer.

The generated `WindowsAppInstaller.exe` can be copied and distributed on its own. The `build` directory and source files are not required on the target computer.

## Usage

1. Select applications directly, through search, or with a profile.
2. Choose install, update, or uninstall.
3. Follow the progress in the status area.

Windows or administrator prompts may appear during installation. Applications are installed sequentially, and a non-zero exit code is treated as an error.

## Technical notes

- The interface uses only the native Win32 API and does not require an additional GUI framework.
- WhatsApp and Apple Music are installed from the `msstore` source; all other packages use the `winget` source.
- The Winget community source is updated before each installation run.
- **Help → App Update** opens the GitHub releases page in the default browser.
- Package and source agreements are accepted automatically, and Winget runs non-interactively.
