#include "MatchmakingClient.hpp"
#include <chrono>
#include <thread>

// Timeout de conexion TCP al servidor de matchmaking (segundos)
static constexpr float CONNECTION_TIMEOUT_S = 5.f;

// Intervalo de polling del socket no-bloqueante mientras se espera pareja (ms)
static constexpr int POLLING_INTERVAL_MS = 100;

void MatchmakingClient::cancel()
{
    m_cancelled = true;
}

std::future<MatchFound> MatchmakingClient::join(const std::string& host, unsigned short port,
                                                  const std::string& username, bool isCompetitive)
{
    m_cancelled = false;

    return std::async(std::launch::async, [=]() -> MatchFound
    {
        // resolve() convierte string a sf::IpAddress (puede resolver DNS)
        const std::optional<sf::IpAddress> address = sf::IpAddress::resolve(host);
        if (!address)
            return { false, "Direccion de servidor invalida" };

        sf::TcpSocket socket;
        if (socket.connect(*address, port, sf::seconds(CONNECTION_TIMEOUT_S))
            != sf::Socket::Status::Done)
            return { false, "No se pudo conectar al servidor de matchmaking" };

        // Enviar peticion de entrada en cola al servidor
        sf::Packet joinPacket;
        const Protocol::MessageType msgType = isCompetitive
            ? Protocol::MessageType::JOIN_COMPETITIVE_QUEUE
            : Protocol::MessageType::JOIN_FRIENDLY_QUEUE;

        joinPacket << static_cast<uint8_t>(msgType) << username;
        if (socket.send(joinPacket) != sf::Socket::Status::Done)
            return { false, "Error al enviar peticion de matchmaking" };

        // Socket no-bloqueante para poder comprobar m_cancelled entre recibos
        socket.setBlocking(false);

        while (!m_cancelled)
        {
            sf::Packet response;
            const sf::Socket::Status status = socket.receive(response);

            if (status == sf::Socket::Status::Done)
            {
                uint8_t rawType{};
                response >> rawType;
                const Protocol::MessageType respType = static_cast<Protocol::MessageType>(rawType);

                if (respType == Protocol::MessageType::MATCH_FOUND)
                {
                    // Deserializar datos del rival y del servidor de juego
                    MatchFound result;
                    result.success = true;
                    uint16_t gamePort{};
                    uint8_t  playerIndex{};
                    response >> result.opponentUsername
                             >> result.opponentRanking
                             >> result.gameServerIP
                             >> gamePort
                             >> result.isCompetitive
                             >> playerIndex;
                    result.gameServerPort   = gamePort;
                    result.localPlayerIndex = playerIndex;
                    return result;
                }

                if (respType == Protocol::MessageType::MATCHMAKING_TIMEOUT)
                    return { false, "Tiempo de busqueda agotado (2 min)" };

                // QUEUED: confirmacion de que estamos en cola, seguir esperando
            }
            else if (status == sf::Socket::Status::Disconnected)
            {
                return { false, "El servidor cerro la conexion" };
            }
            // NotReady: sin datos aun; dormir antes del proximo intento

            std::this_thread::sleep_for(std::chrono::milliseconds(POLLING_INTERVAL_MS));
        }

        // Usuario cancelo: notificar al servidor y cerrar la conexion
        socket.setBlocking(true);
        sf::Packet cancelPacket;
        cancelPacket << static_cast<uint8_t>(Protocol::MessageType::CANCEL_MATCHMAKING);
        socket.send(cancelPacket); // Best-effort: ignoramos posible fallo de envio

        return { false, "Busqueda cancelada" };
    });
}
