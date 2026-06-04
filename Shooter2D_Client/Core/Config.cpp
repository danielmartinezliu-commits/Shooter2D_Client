#include "Config.hpp"
#include <fstream>
#include <iostream>

Config& Config::instance()
{
    static Config cfg;
    return cfg;
}

void Config::load(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cout << "[CONFIG] No se encontro " << filePath << " -> usando localhost por defecto\n";
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        // Ignorar comentarios y lineas vacias
        if (line.empty() || line[0] == '#')
            continue;

        const std::string::size_type eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        const std::string key = line.substr(0, eq);
        const std::string val = line.substr(eq + 1);

        if (key == "server_ip") serverIP = val;
        if (key == "auth_port") authPort = static_cast<unsigned short>(std::stoi(val));
        if (key == "game_port") gamePort = static_cast<unsigned short>(std::stoi(val));
    }

    std::cout << "[CONFIG] Servidor: " << serverIP << "  auth=" << authPort << "  game=" << gamePort << "\n";
}
