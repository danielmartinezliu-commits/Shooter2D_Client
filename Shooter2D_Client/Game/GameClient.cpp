#include "GameClient.hpp"
#include "../Network/Protocol.hpp"
#include <cstring>
#include <iostream>

// ---- helpers de serializacion de buffer crudo -------------------------
static std::size_t wU8 (char* b, std::size_t o, uint8_t  v){ std::memcpy(b+o,&v,1); return o+1; }
static std::size_t wU16(char* b, std::size_t o, uint16_t v){ std::memcpy(b+o,&v,2); return o+2; }
static std::size_t wF32(char* b, std::size_t o, float    v){ std::memcpy(b+o,&v,4); return o+4; }
static std::size_t wStr(char* b, std::size_t o, const std::string& s)
{
    uint8_t len = static_cast<uint8_t>(s.size());
    std::memcpy(b+o, &len, 1);
    std::memcpy(b+o+1, s.c_str(), len);
    return o+1+len;
}
static std::size_t rU8 (const char* b, std::size_t o, uint8_t&  v){ std::memcpy(&v,b+o,1); return o+1; }
static std::size_t rU16(const char* b, std::size_t o, uint16_t& v){ std::memcpy(&v,b+o,2); return o+2; }
static std::size_t rF32(const char* b, std::size_t o, float&    v){ std::memcpy(&v,b+o,4); return o+4; }
// -----------------------------------------------------------------------

bool GameClient::connect(const std::string& serverIp, uint16_t lobbyPort,
                          const std::string& username, uint8_t playerIndex,
                          bool isCompetitive)
{
    const std::optional<sf::IpAddress> addr = sf::IpAddress::resolve(serverIp);
    if (!addr)
    {
        std::cerr << "[GAME] Direccion invalida: " << serverIp << "\n";
        return false;
    }
    m_serverAddr  = addr;
    m_playerIndex = playerIndex;

    if (m_udp.bind(0) != sf::Socket::Status::Done)
    {
        std::cerr << "[GAME] No se pudo vincular el socket UDP\n";
        return false;
    }

    // Construir GAME_JOIN en buffer crudo
    char        joinBuf[256];
    std::size_t joinSize = 0;
    joinSize = wU8 (joinBuf, joinSize, static_cast<uint8_t>(Protocol::MessageType::GAME_JOIN));
    joinSize = wStr(joinBuf, joinSize, username);
    joinSize = wU8 (joinBuf, joinSize, playerIndex);
    joinSize = wU8 (joinBuf, joinSize, static_cast<uint8_t>(isCompetitive));

    sf::SocketSelector sel;
    sel.add(m_udp);

    for (int attempt = 0; attempt < MAX_JOIN_ATTEMPTS; ++attempt)
    {
        m_udp.send(joinBuf, joinSize, *addr, lobbyPort);
        if (!sel.wait(sf::seconds(JOIN_RETRY_TIMEOUT_S))) continue;

        char        readyBuf[16];
        std::size_t readyReceived = 0;
        std::optional<sf::IpAddress> sender;
        uint16_t senderPort{};
        if (m_udp.receive(readyBuf, sizeof(readyBuf), readyReceived, sender, senderPort)
            != sf::Socket::Status::Done) continue;

        uint8_t  type{};
        uint16_t sessionPort{};
        std::size_t off = 0;
        off = rU8 (readyBuf, off, type);
        off = rU16(readyBuf, off, sessionPort);

        if (type != static_cast<uint8_t>(Protocol::MessageType::GAME_READY)) continue;

        m_sessionPort = sessionPort;
        m_udp.setBlocking(false);
        m_connected = true;
        std::cout << "[GAME] Conectado | sesion UDP port=" << sessionPort << "\n";
        return true;
    }

    std::cerr << "[GAME] No se recibio GAME_READY tras " << MAX_JOIN_ATTEMPTS << " intentos\n";
    return false;
}

// ================================================================
// SISTEMA 1 — PREDICCION DEL CLIENTE
// ================================================================
void GameClient::sendInput(uint16_t seq, InputFlags flags, float dt)
{
    if (!m_connected) return;
    char        buf[16];
    std::size_t off = 0;
    const uint8_t flagByte = flags.toUint8();
    off = wU8 (buf, off, static_cast<uint8_t>(Protocol::MessageType::PLAYER_INPUT));
    off = wU8 (buf, off, m_playerIndex);
    off = wU16(buf, off, seq);
    off = wU8 (buf, off, flagByte);
    off = wF32(buf, off, dt);
    m_udp.send(buf, off, *m_serverAddr, m_sessionPort);
}

void GameClient::sendBullet(float bx, float by, bool facingRight)
{
    if (!m_connected) return;
    char        buf[16];
    std::size_t off = 0;
    off = wU8 (buf, off, static_cast<uint8_t>(Protocol::MessageType::PLAYER_SHOOT));
    off = wU8 (buf, off, m_playerIndex);
    off = wF32(buf, off, bx);
    off = wF32(buf, off, by);
    off = wU8 (buf, off, static_cast<uint8_t>(facingRight));
    m_udp.send(buf, off, *m_serverAddr, m_sessionPort);
}

void GameClient::tickPing(float dt)
{
    if (!m_connected) return;
    m_pingTimer += dt;
    if (m_pingTimer < PING_INTERVAL) return;
    m_pingTimer = 0.f;
    char    buf[1];
    wU8(buf, 0, static_cast<uint8_t>(Protocol::MessageType::PING));
    m_udp.send(buf, 1, *m_serverAddr, m_sessionPort);
}

void GameClient::sendAck(uint16_t seqNum)
{
    char        buf[4];
    std::size_t off = 0;
    off = wU8 (buf, off, static_cast<uint8_t>(Protocol::MessageType::ACK));
    off = wU16(buf, off, seqNum);
    m_udp.send(buf, off, *m_serverAddr, m_sessionPort);
}

// ================================================================
// receiveAll: procesa todos los paquetes UDP pendientes.
// ================================================================
void GameClient::receiveAll()
{
    if (!m_connected) return;

    char        buf[512];
    std::size_t received = 0;
    std::optional<sf::IpAddress> sender;
    uint16_t senderPort{};

    while (m_udp.receive(buf, sizeof(buf), received, sender, senderPort)
           == sf::Socket::Status::Done)
    {
        std::size_t off = 0;
        uint8_t msgType{};
        off = rU8(buf, off, msgType);
        const Protocol::MessageType t = static_cast<Protocol::MessageType>(msgType);

        if (t == Protocol::MessageType::PONG)
        {
            // El servidor esta vivo; no se necesita accion adicional.
        }
        else if (t == Protocol::MessageType::GAME_ABANDON)
        {
            GameEvent ev;
            ev.type = msgType;
            off = rU8(buf, off, ev.p1);
            m_eventQueue.push_back(ev);
        }
        else if (t == Protocol::MessageType::PLAYER_POS
              || t == Protocol::MessageType::PLAYER_SHOOT)
        {
            RemoteUpdate upd;
            upd.type = msgType;
            uint8_t facing{};
            off = rU8 (buf, off, upd.playerIndex);
            off = rF32(buf, off, upd.x);
            off = rF32(buf, off, upd.y);
            off = rU8 (buf, off, facing);
            upd.facingRight = static_cast<bool>(facing);
            m_updateQueue.push_back(upd);
        }
        else if (t == Protocol::MessageType::PLAYER_STATE)
        {
            PlayerStateUpdate su;
            uint8_t grounded{};
            off = rU8 (buf, off, su.playerIndex);
            off = rU16(buf, off, su.ackSeq);
            off = rF32(buf, off, su.x);
            off = rF32(buf, off, su.y);
            off = rF32(buf, off, su.vy);
            off = rU8 (buf, off, grounded);
            su.grounded = static_cast<bool>(grounded);
            m_stateQueue.push_back(su);
        }
        else if (t == Protocol::MessageType::PLAYER_HIT
              || t == Protocol::MessageType::GAME_OVER
              || t == Protocol::MessageType::PLAYER_RESPAWN)
        {
            uint16_t seqNum{};
            off = rU16(buf, off, seqNum);
            sendAck(seqNum);

            if (m_seenReliable.count(seqNum)) continue;
            m_seenReliable.insert(seqNum);

            GameEvent ev;
            ev.type = msgType;
            if (t == Protocol::MessageType::PLAYER_HIT)
            {
                off = rU8(buf, off, ev.p1);
                off = rU8(buf, off, ev.p2);
                off = rU8(buf, off, ev.p3);
            }
            else if (t == Protocol::MessageType::PLAYER_RESPAWN)
            {
                off = rU8 (buf, off, ev.p1);
                off = rF32(buf, off, ev.x);
                off = rF32(buf, off, ev.y);
            }
            else if (t == Protocol::MessageType::GAME_OVER)
            {
                off = rU8(buf, off, ev.p1);
            }
            m_eventQueue.push_back(ev);
        }
    }
}

std::optional<RemoteUpdate> GameClient::poll()
{
    if (m_updateQueue.empty()) return std::nullopt;
    const RemoteUpdate upd = m_updateQueue.front();
    m_updateQueue.pop_front();
    return upd;
}

std::optional<GameEvent> GameClient::pollEvent()
{
    if (m_eventQueue.empty()) return std::nullopt;
    const GameEvent ev = m_eventQueue.front();
    m_eventQueue.pop_front();
    return ev;
}

std::optional<PlayerStateUpdate> GameClient::pollStateUpdate()
{
    if (m_stateQueue.empty()) return std::nullopt;
    const PlayerStateUpdate su = m_stateQueue.front();
    m_stateQueue.pop_front();
    return su;
}
