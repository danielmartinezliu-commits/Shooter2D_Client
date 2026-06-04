#pragma once
#include <SFML/Network.hpp>
#include <deque>
#include <optional>
#include <string>
#include <unordered_set>
#include <cstdint>

// Actualizacion de estado de un jugador recibida por UDP (no fiable).
struct RemoteUpdate
{
    uint8_t type; // PLAYER_POS o PLAYER_SHOOT
    uint8_t playerIndex; // Indice del jugador origen (0 o 1)
    float x, y; // Posicion o punto de origen de la bala
    bool facingRight; // Direccion del jugador
};

// Evento critico de juego recibido por UDP fiable.
struct GameEvent
{
    uint8_t type; // PLAYER_HIT, PLAYER_RESPAWN o GAME_OVER
    uint8_t p1 = 0; // victimIndex / playerIndex / loserIndex segun el tipo
    uint8_t p2 = 0; // newHp (en PLAYER_HIT)
    uint8_t p3 = 0; // newLives (en PLAYER_HIT)
    float x = 0.f; // Coordenada X de resurreccion (en PLAYER_RESPAWN)
    float y = 0.f; // Coordenada Y de resurreccion (en PLAYER_RESPAWN)
};

class GameClient
{
public:
    //Conecta al servidor de juego mediante el handshake UDP GAME_JOIN/GAME_READY.
    bool connect(const std::string& serverIp, uint16_t lobbyPort, const std::string& username, uint8_t playerIndex);

    // Envia la posicion del jugador local al servidor (UDP no fiable).
    void sendPosition(float x, float y, bool facingRight);

    // Envia un disparo del jugador local al servidor (UDP no fiable).
    void sendBullet(float bx, float by, bool facingRight);

    // Lee todos los paquetes UDP pendientes y los distribuye en las colas internas.
    void receiveAll();

    // Extrae el siguiente RemoteUpdate de la cola (PLAYER_POS o PLAYER_SHOOT).
    std::optional<RemoteUpdate> poll();

    // Extrae el siguiente GameEvent de la cola (HIT, RESPAWN, GAME_OVER).
    std::optional<GameEvent> pollEvent();

    // Indica si la conexion con el servidor de juego esta establecida.
    bool isConnected() const { return m_connected; }

private:
    // Envia un paquete ACK al servidor para confirmar un evento fiable.
    void sendAck(uint16_t seqNum);

    // Constantes de conexion
    static constexpr int MAX_JOIN_ATTEMPTS = 10; // Maximos reintentos de GAME_JOIN
    static constexpr float JOIN_RETRY_TIMEOUT_S = 1.f; // Timeout por intento de GAME_JOIN (s)

    // Estado de red
    sf::UdpSocket m_udp; // Socket UDP unico para toda la partida
    std::optional<sf::IpAddress> m_serverAddr; // Direccion IP del servidor de sesion
    uint16_t m_sessionPort = 0; // Puerto UDP de la sesion
    uint8_t m_playerIndex = 0; // Indice de este cliente (0 o 1)
    bool m_connected   = false;

    // Colas de recepcion
    std::deque<RemoteUpdate> m_updateQueue; // Actualizaciones de posicion/disparo del rival
    std::deque<GameEvent> m_eventQueue; // Eventos criticos del servidor
    std::unordered_set<uint16_t> m_seenReliable; // SeqNums ya procesados (evita duplicados)
};
