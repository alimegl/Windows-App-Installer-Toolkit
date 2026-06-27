#include "winappinstaller.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand) {
    return RunWindowsAppInstaller(instance, showCommand);
}
