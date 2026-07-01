cmake_minimum_required(VERSION 3.20)

project(WindowsAppInstallerGui VERSION 2.0.0 LANGUAGES CXX RC)

add_executable(
    WindowsAppInstaller
    WIN32
    main.cpp
    applications.h
    settings.cpp
    settings.h
    winget_service.cpp
    winget_service.h
    winappinstaller.cpp
    winappinstaller.h
    resource.h
    app.rc
)

target_compile_features(WindowsAppInstaller PRIVATE cxx_std_17)
target_compile_definitions(
    WindowsAppInstaller
    PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0A00
)
target_link_libraries(WindowsAppInstaller PRIVATE advapi32 comctl32 dwmapi uxtheme)

if(MSVC)
    target_compile_options(WindowsAppInstaller PRIVATE /W4 /permissive-)
    set_property(
        TARGET WindowsAppInstaller
        PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
elseif(MINGW)
    target_compile_options(WindowsAppInstaller PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(WindowsAppInstaller PRIVATE -static -static-libgcc -static-libstdc++)
endif()

install(TARGETS WindowsAppInstaller RUNTIME DESTINATION .)
