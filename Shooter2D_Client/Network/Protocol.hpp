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

        //Launcher: verificacion de mapa (TCP, antes del login)
        MAP_VERSION_CHECK = 96,   // Client->Server [uint32: hash FNV-1a del mapa local]
        MAP_UP_TO_DATE = 97,   // Server->Client (sin payload)
        MAP_UPDATE = 98,   // Server->Client [string: mapData]
    };
}
