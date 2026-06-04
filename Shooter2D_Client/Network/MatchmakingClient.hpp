#pragma once
#include <SFML/Network.hpp>
#include <atomic>
#include <future>
#include <string>
#include <cstdint>
#include "Protocol.hpp"

struct MatchFound
{
    bool success = false; // true si se encontro pareja
    std::string message; // Mensaje de error si success=false
    std::string opponentUsername; // Nombre del rival
    int opponentRanking = 0; // Ranking del rival
    std::string gameServerIP; // IP del servidor de juego
    uint16_t gameServerPort = 0; // Puerto UDP del lobby del servidor de juego
    bool isCompetitive = false; // true si la partida es competitiva
    uint8_t localPlayerIndex = 0;    // Spawn asignado a este cliente (0 o 1)
};

class MatchmakingClient
{
public:
    // Inicia la busqueda de partida de forma asincrona.
  
    std::future<MatchFound> join(const std::string& host, unsigned short port, const std::string& username, bool isCompetitive);

    // Senaliza al hilo de fondo que debe cancelar la busqueda.
    void cancel();

private:
    std::atomic<bool> m_cancelled{ false }; // Flag de cancelacion consultado por el hilo de fondo
};
