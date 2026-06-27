#include "winappinstaller.h"

#include <cstdlib>
#include <iostream>
#include <limits>

void mainMenu() {
    while (true) {
        std::cout << "Welcome to the Windows App Installer Toolkit!" << std::endl;
        std::cout << "Please select an option:" << std::endl;
        std::cout << "1. Install an application" << std::endl;
        std::cout << "2. Exit" << std::endl;

        int choice;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Invalid choice. Please enter a number." << std::endl;
            continue;
        }

        switch (choice) {
            case 1:
                installApplication();
                break;
            case 2:
                return;
            default:
                std::cout << "Invalid choice. Please try again." << std::endl;
        }
    }
}

void installApplication() {
    std::cout << "Available applications for installation:" << std::endl;
    std::cout << "1. Discord" << std::endl;
    std::cout << "2. Whatsapp" << std::endl;
    std::cout << "3. Apple Music" << std::endl;
    std::cout << "4. Streamlabs OBS" << std::endl;
    std::cout << "5. Steam" << std::endl;
    std::cout << "6. Ubisoft Connect" << std::endl;
    std::cout << "7. EA App" << std::endl;
    std::cout << "8. Epic Games Launcher" << std::endl;
    std::cout << "9. Rockstar Games Launcher" << std::endl;
    std::cout << "10. Visual Studio Code" << std::endl;
    std::cout << "11. Google Chrome" << std::endl;
    std::cout << "12. MSYS2" << std::endl;
    std::cout << "13. Git" << std::endl;
    std::cout << std::endl;
    std::cout << "Do you want to install all applications? (y/n): ";

    char installAll;
    std::cin >> installAll;

    if (installAll == 'y' || installAll == 'Y') {
        std::cout << "Updating the Winget community source..." << std::endl;
        std::system("winget source update --name winget");
        std::cout << "Installing all applications..." << std::endl;
        std::system("winget install -e --id Discord.Discord --source winget");
        std::system("winget install -e --id WhatsApp.WhatsApp --source winget");
        std::system("winget install -e --id Apple.Music --source winget");
        std::system("winget install -e --id Streamlabs.Streamlabs --source winget");
        std::system("winget install -e --id Valve.Steam --source winget");
        std::system("winget install -e --id Ubisoft.Connect --source winget");
        std::system("winget install -e --id ElectronicArts.EADesktop --source winget");
        std::system("winget install -e --id EpicGames.EpicGamesLauncher --source winget");
        std::system("winget install -e --id RockstarGames.RockstarGames.Launcher --source winget");
        std::system("winget install -e --id Microsoft.VisualStudioCode --source winget");
        std::system("winget install -e --id Google.Chrome --source winget");
        std::system("winget install -e --id MSYS2.MSYS2 --source winget");
        std::system("winget install -e --id Git.Git --source winget");
        return;
    }

    if (installAll != 'n' && installAll != 'N') {
        std::cout << "Invalid choice. Please enter y or n." << std::endl;
        return;
    }

    std::cout << "Please select one application to install (1-13): ";

    int applicationChoice;
    if (!(std::cin >> applicationChoice)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Invalid choice. Please enter a number." << std::endl;
        return;
    }

    if (applicationChoice < 1 || applicationChoice > 13) {
        std::cout << "Invalid application number. Please choose 1-13." << std::endl;
        return;
    }

    std::cout << "Updating the Winget community source..." << std::endl;
    std::system("winget source update --name winget");

    switch (applicationChoice) {
        case 1:
            std::cout << "Installing Discord..." << std::endl;
            std::system("winget install -e --id Discord.Discord --source winget");
            break;
        case 2:
            std::cout << "Installing Whatsapp..." << std::endl;
            std::system("winget install -e --id WhatsApp.WhatsApp --source winget");
            break;
        case 3:
            std::cout << "Installing Apple Music..." << std::endl;
            std::system("winget install -e --id Apple.Music --source winget");
            break;
        case 4:
            std::cout << "Installing Streamlabs OBS..." << std::endl;
            std::system("winget install -e --id Streamlabs.Streamlabs --source winget");
            break;
        case 5:
            std::cout << "Installing Steam..." << std::endl;
            std::system("winget install -e --id Valve.Steam --source winget");
            break;
        case 6:
            std::cout << "Installing Ubisoft Connect..." << std::endl;
            std::system("winget install -e --id Ubisoft.Connect --source winget");
            break;
        case 7:
            std::cout << "Installing EA App..." << std::endl;
            std::system("winget install -e --id ElectronicArts.EADesktop --source winget");
            break;
        case 8:
            std::cout << "Installing Epic Games Launcher..." << std::endl;
            std::system("winget install -e --id EpicGames.EpicGamesLauncher --source winget");
            break;
        case 9:
            std::cout << "Installing Rockstar Games Launcher..." << std::endl;
            std::system("winget install -e --id RockstarGames.RockstarGames.Launcher --source winget");
            break;
        case 10:
            std::cout << "Installing Visual Studio Code..." << std::endl;
            std::system("winget install -e --id Microsoft.VisualStudioCode --source winget");
            break;
        case 11:
            std::cout << "Installing Google Chrome..." << std::endl;
            std::system("winget install -e --id Google.Chrome --source winget");
            break;
        case 12:
            std::cout << "Installing MSYS2..." << std::endl;
            std::system("winget install -e --id MSYS2.MSYS2 --source winget");
            break;
        case 13:
            std::cout << "Installing Git..." << std::endl;
            std::system("winget install -e --id Git.Git --source winget");
            break;
        default:
            std::cout << "Invalid application number. Please choose 1-13." << std::endl;
    }
}
