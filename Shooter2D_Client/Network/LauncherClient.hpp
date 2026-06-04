#pragma once
#include <string>

class LauncherClient
{
public:
    // Comprueba el mapa local y lo actualiza si es necesario.
    bool checkAndUpdate(const std::string& host, unsigned short port, const std::string& mapPath);
};
