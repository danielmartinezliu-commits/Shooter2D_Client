#include "LauncherClient.hpp"
#include "Protocol.hpp"
#include <SFML/Network.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

static uint32_t computeHash(const std::string& data)
{
    // FNV-1a 32 bits: mismo algoritmo que el servidor.
    uint32_t hash = 2166136261u;
    for (const unsigned char c : data)
    {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

bool LauncherClient::checkAndUpdate(const std::string& host, unsigned short port, const std::string& mapPath)
{
    // Calcular hash del mapa local (0 si no existe todavia)
    uint32_t localHash = 0;
    {
        std::ifstream mf(mapPath);
        if (mf.is_open())
        {
            std::ostringstream ss;
            ss << mf.rdbuf();
            localHash = computeHash(ss.str());
        }
    }

    const std::optional<sf::IpAddress> addr = sf::IpAddress::resolve(host);
    if (!addr)
    {
        std::cerr << "[LAUNCHER] No se pudo resolver la IP del servidor\n";
        return false;
    }

    sf::TcpSocket socket;
    if (socket.connect(*addr, port) != sf::Socket::Status::Done)
    {
        std::cerr << "[LAUNCHER] No se pudo conectar al servidor\n";
        return false;
    }

    sf::Packet request;
    request << static_cast<uint8_t>(Protocol::MessageType::MAP_VERSION_CHECK) << localHash;
    if (socket.send(request) != sf::Socket::Status::Done)
        return false;

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
        return false;

    uint8_t rawType{};
    response >> rawType;
    const Protocol::MessageType type = static_cast<Protocol::MessageType>(rawType);

    if (type == Protocol::MessageType::MAP_UP_TO_DATE)
    {
        std::cout << "[LAUNCHER] Mapa al dia\n";
        return true;
    }

    if (type == Protocol::MessageType::MAP_UPDATE)
    {
        std::string mapData;
        response >> mapData;

        std::ofstream mf(mapPath);
        if (!mf.is_open())
        {
            std::cerr << "[LAUNCHER] No se pudo escribir mapa.txt\n";
            return false;
        }
        mf << mapData;

        std::cout << "[LAUNCHER] Mapa actualizado\n";
        return true;
    }

    return false;
}
