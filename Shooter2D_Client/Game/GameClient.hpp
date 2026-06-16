#pragma once
#include <SFML/Network.hpp>
#include <deque>
#include <optional>
#include <string>
#include <unordered_set>
#include <cstdint>
#include "Player.hpp" // InputFlags

// Actualizacion de posicion/disparo del rival (UDP no fiable).
// Usada por el jugador remoto para el buffer de interpolacion.
struct RemoteUpdate
{
    uint8_t type;        // PLAYER_POS o PLAYER_SHOOT
    uint8_t playerIndex;
    float   x, y;        // Posicion (POS) u origen de bala (SHOOT)
    bool    facingRight;
};

// Evento critico de juego (UDP fiable con ACK + deduplicacion).
struct GameEvent
{
    uint8_t type;
    uint8_t p1 = 0;  // victimIndex / playerIndex / loserIndex
    uint8_t p2 = 0;  // newHp (PLAYER_HIT)
    uint8_t p3 = 0;  // newLives (PLAYER_HIT)
    float   x  = 0.f; // Posicion X de respawn (PLAYER_RESPAWN)
    float   y  = 0.f; // Posicion Y de respawn (PLAYER_RESPAWN)
};

// ================================================================
// PlayerStateUpdate — respuesta del servidor al PLAYER_INPUT.
//
// SISTEMA 3 — RECONCILIACION:
// El servidor envia PLAYER_STATE ~20 Hz con su posicion autoritativa
// para el ultimo input que proceso. GameScreen lo pasa a
// Player::reconcile() para detectar y corregir errores de prediccion.
// ================================================================
struct PlayerStateUpdate
{
    uint8_t  playerIndex;
    uint16_t ackSeq;   // Ultimo seq de PLAYER_INPUT procesado por el servidor
    float    x, y;     // Posicion autoritativa
    float    vy;       // Velocidad vertical (necesaria para re-simulacion)
    bool     grounded; // En el suelo (necesario para prediccion de saltos)
};

class GameClient
{
public:
    // @brief Handshake UDP GAME_JOIN/GAME_READY con el servidor de juego.
    // @param isCompetitive Se incluye en GAME_JOIN para que el GameServer
    //                      sepa si debe reportar el resultado al ranking.
    bool connect(const std::string& serverIp, uint16_t lobbyPort,
                 const std::string& username, uint8_t playerIndex,
                 bool isCompetitive);

    // @brief  SISTEMA 1 — PREDICCION.
    //         Envia el input del frame actual al servidor.
    //         El servidor usa estos datos para simular la fisica autoritativa.
    // @param  seq   Numero de secuencia devuelto por Player::applyInput().
    // @param  flags Teclas pulsadas en este frame.
    // @param  dt    Delta-time del frame.
    void sendInput(uint16_t seq, InputFlags flags, float dt);

    // @brief Envia un disparo al servidor (UDP no fiable).
    //        El servidor crea el ServerBullet y hace relay al rival.
    void sendBullet(float bx, float by, bool facingRight);

    // @brief Envia PING al servidor cada PING_INTERVAL segundos.
    //        Llamar una vez por frame con el dt del frame.
    //        El servidor responde con PONG y usa los PING para detectar
    //        desconexiones (si no llega nada en DISCONNECT_TIMEOUT s).
    void tickPing(float dt);

    // @brief Lee todos los paquetes UDP pendientes y los distribuye en colas.
    void receiveAll();

    // @brief Extrae la siguiente actualizacion de posicion/disparo del rival.
    std::optional<RemoteUpdate> poll();

    // @brief Extrae el siguiente evento critico (HIT, RESPAWN, GAME_OVER).
    std::optional<GameEvent> pollEvent();

    // @brief  SISTEMA 3 — RECONCILIACION.
    //         Extrae el siguiente PLAYER_STATE del servidor.
    //         GameScreen lo pasa a Player::reconcile() cada frame.
    std::optional<PlayerStateUpdate> pollStateUpdate();

    bool isConnected() const { return m_connected; }

private:
    // @brief Envia ACK para confirmar un evento fiable.
    void sendAck(uint16_t seqNum);

    static constexpr int   MAX_JOIN_ATTEMPTS    = 10;
    static constexpr float JOIN_RETRY_TIMEOUT_S =  1.f;
    static constexpr float PING_INTERVAL        =  2.f; // Segundos entre PINGs

    sf::UdpSocket              m_udp;
    std::optional<sf::IpAddress> m_serverAddr;
    uint16_t m_sessionPort = 0;
    uint8_t  m_playerIndex = 0;
    bool     m_connected   = false;
    float    m_pingTimer   = 0.f;

    // Colas de recepcion
    std::deque<RemoteUpdate>      m_updateQueue;  // Posicion/disparo del rival
    std::deque<GameEvent>         m_eventQueue;   // Eventos criticos fiables
    std::deque<PlayerStateUpdate> m_stateQueue;   // PLAYER_STATE para reconciliacion
    std::unordered_set<uint16_t>  m_seenReliable; // SeqNums ya procesados
};
