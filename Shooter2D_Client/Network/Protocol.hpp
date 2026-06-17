#pragma once
#include <cstdint>

namespace Protocol
{
    enum class MessageType : uint8_t
    {
        // Autenticacion (Client -> Server)
        LOGIN_REQUEST = 1,
        REGISTER_REQUEST = 2,

        //Autenticacion (Server -> Client)
        AUTH_SUCCESS = 16,
        AUTH_FAIL = 17,

        //Matchmaking (Client -> Server)
        JOIN_FRIENDLY_QUEUE = 32,
        JOIN_COMPETITIVE_QUEUE = 33,
        CANCEL_MATCHMAKING = 34,

        //Matchmaking (Server -> Client)
        QUEUED = 48,
        MATCH_FOUND = 49,
        MATCHMAKING_TIMEOUT = 50,

        //Partida: handshake UDP
        GAME_JOIN = 80,   // Client->Lobby  [string: username][uint8: playerIndex]
        GAME_READY = 81,   // Server->Client [uint16: sessionPort]

        //Partida: estado (UDP no fiable)
        PLAYER_POS = 82,   // [uint8: playerIndex][float: x][float: y][uint8: facingRight]
        PLAYER_SHOOT = 83,   // [uint8: playerIndex][float: bx][float: by][uint8: facingRight]

        //Partida: eventos criticos (UDP fiable)
        // Formato: [uint8: type][uint16: seqNum][...payload...]
        // El receptor responde con ACK para garantizar entrega.
        PLAYER_HIT = 84,   // [uint16: seqNum][uint8: victimIndex][uint8: newHp][uint8: newLives]
        GAME_OVER = 85,   // [uint16: seqNum][uint8: loserIndex]
        PLAYER_RESPAWN = 86,   // [uint16: seqNum][uint8: playerIndex][float: x][float: y]

        //Confirmacion de evento fiable (Client -> Server)
        ACK = 87,   // [uint16: seqNum]

        // ================================================================
        // SISTEMAS DE RED AVANZADOS
        //
        // Flujo completo por frame:
        //   1. Cliente aplica input localmente sin esperar al servidor
        //      => PREDICCION DEL CLIENTE
        //   2. Cliente envia PLAYER_INPUT al servidor cada frame.
        //   3. Servidor simula la misma fisica con esos inputs
        //      => VALIDACION DEL SERVIDOR
        //   4. Servidor envia PLAYER_STATE ~20 Hz con posicion autoritativa.
        //   5. Cliente compara su prediccion con PLAYER_STATE.
        //      Si difieren: corrige y re-simula inputs pendientes
        //      => RECONCILIACION
        //   6. El otro cliente recibe PLAYER_POS relay y lo almacena en
        //      un buffer con timestamp para renderizar 100ms en el pasado
        //      => INTERPOLACION
        // ================================================================

        // PLAYER_INPUT — Cliente -> Servidor, cada frame.
        // Lleva las teclas pulsadas y el delta-time del frame para que
        // el servidor replique exactamente el mismo paso de fisica.
        // [uint8: playerIndex][uint16: seq][uint8: flags][float: dt]
        //   seq   = numero de secuencia monotono (identifica el frame)
        //   flags = bitmask FLAG_LEFT | FLAG_RIGHT | FLAG_JUMP (ver abajo)
        //   dt    = delta-time del cliente, acotado a 0.1s en el servidor
        PLAYER_INPUT = 0x58,

        // PLAYER_STATE — Servidor -> Cliente, ~20 Hz.
        // Posicion autoritativa para que el cliente detecte y corrija
        // divergencias de prediccion (reconciliacion).
        // [uint8: playerIndex][uint16: ackSeq][float: x][float: y][float: vy][uint8: grounded]
        //   ackSeq  = ultimo seq de PLAYER_INPUT procesado por el servidor
        //   vy      = velocidad vertical (para re-simulacion precisa)
        //   grounded= 1 si el jugador esta en el suelo (afecta saltos)
        PLAYER_STATE = 0x59,

        // Heartbeat UDP (Client <-> GameServer).
        // El cliente envia PING cada PING_INTERVAL segundos.
        // El servidor responde con PONG. Si el servidor no recibe ningun
        // paquete de un jugador en DISCONNECT_TIMEOUT segundos, considera
        // que se desconecto y manda GAME_ABANDON al rival.
        PING         = 90, // Client->Server (sin payload)
        PONG         = 91, // Server->Client (sin payload)

        // Notificacion de abandono. El servidor envia esto al jugador
        // superviviente cuando detecta que el rival dejo de responder.
        // [uint8: disconnectedPlayerIndex]
        GAME_ABANDON = 92,

        // Burla: animacion + sonido, sin efecto en el juego (UDP no fiable).
        // [uint8: playerIndex] -- Client->Server y Server->Client (relay)
        PLAYER_TAUNT = 94,

        //Launcher: verificacion de mapa (TCP, antes del login)
        MAP_VERSION_CHECK = 96,   // Client->Server [uint32: hash FNV-1a del mapa local]
        MAP_UP_TO_DATE = 97,   // Server->Client (sin payload)
        MAP_UPDATE = 98,   // Server->Client [string: mapData]
    };

    // ----------------------------------------------------------------
    // Mascaras de bits para el campo 'flags' de PLAYER_INPUT.
    // MISMO encoding en cliente y servidor — garantiza que la fisica
    // del servidor reproduce exactamente la del cliente.
    // ----------------------------------------------------------------
    constexpr uint8_t FLAG_LEFT  = 0x01; // Tecla izquierda / A
    constexpr uint8_t FLAG_RIGHT = 0x02; // Tecla derecha   / D
    constexpr uint8_t FLAG_JUMP  = 0x04; // Tecla arriba    / W
}
