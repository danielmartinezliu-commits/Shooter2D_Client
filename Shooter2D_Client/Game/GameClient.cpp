#include "GameClient.hpp"
#include "../Network/Protocol.hpp"
#include <iostream>

bool GameClient::connect(const std::string& serverIp, uint16_t lobbyPort,
                          const std::string& username, uint8_t playerIndex)
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

    // Construir GAME_JOIN una sola vez; se reusa en cada reintento
    sf::Packet joinPacket;
    joinPacket << static_cast<uint8_t>(Protocol::MessageType::GAME_JOIN)
               << username
               << playerIndex;

    // Enviar GAME_JOIN con reintentos hasta recibir GAME_READY del servidor
    sf::SocketSelector sel;
    sel.add(m_udp);

    for (int attempt = 0; attempt < MAX_JOIN_ATTEMPTS; ++attempt)
    {
        m_udp.send(joinPacket, *addr, lobbyPort);

        if (!sel.wait(sf::seconds(JOIN_RETRY_TIMEOUT_S))) continue; // Timeout -> reintentar

        sf::Packet readyPacket;
        std::optional<sf::IpAddress> sender;
        uint16_t senderPort{};

        if (m_udp.receive(readyPacket, sender, senderPort) != sf::Socket::Status::Done)
            continue;

        uint8_t  type{};
        uint16_t sessionPort{};
        readyPacket >> type >> sessionPort;

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

void GameClient::sendPosition(float x, float y, bool facingRight)
{
    if (!m_connected) return;
    sf::Packet packet;
    packet << static_cast<uint8_t>(Protocol::MessageType::PLAYER_POS) << m_playerIndex << x << y << static_cast<uint8_t>(facingRight);
    m_udp.send(packet, *m_serverAddr, m_sessionPort);
}

void GameClient::sendBullet(float bx, float by, bool facingRight)
{
    if (!m_connected) return;
    sf::Packet packet;
    packet << static_cast<uint8_t>(Protocol::MessageType::PLAYER_SHOOT) << m_playerIndex << bx << by << static_cast<uint8_t>(facingRight);
    m_udp.send(packet, *m_serverAddr, m_sessionPort);
}

void GameClient::sendAck(uint16_t seqNum)
{
    sf::Packet ack;
    ack << static_cast<uint8_t>(Protocol::MessageType::ACK) << seqNum;
    m_udp.send(ack, *m_serverAddr, m_sessionPort);
}

void GameClient::receiveAll()
{
    if (!m_connected) return;

    sf::Packet packet;
    std::optional<sf::IpAddress> sender;
    uint16_t senderPort{};

    while (m_udp.receive(packet, sender, senderPort) == sf::Socket::Status::Done)
    {
        uint8_t msgType{};
        packet >> msgType;
        const Protocol::MessageType t = static_cast<Protocol::MessageType>(msgType);

        if (t == Protocol::MessageType::PLAYER_POS || t == Protocol::MessageType::PLAYER_SHOOT)
        {
            // Mensajes no fiables: posicion y disparo del rival
            RemoteUpdate upd;
            upd.type = msgType;
            uint8_t facing{};
            packet >> upd.playerIndex >> upd.x >> upd.y >> facing;
            upd.facingRight = static_cast<bool>(facing);
            m_updateQueue.push_back(upd);
        }
        else if (t == Protocol::MessageType::PLAYER_HIT || t == Protocol::MessageType::GAME_OVER || t == Protocol::MessageType::PLAYER_RESPAWN)
        {
            // Mensajes fiables: leer seqNum y responder con ACK siempre
            uint16_t seqNum{};
            packet >> seqNum;
            sendAck(seqNum); // Confirmar recepcion aunque sea duplicado

            // Descartar duplicados ya procesados
            if (m_seenReliable.count(seqNum)) continue;
            m_seenReliable.insert(seqNum);

            GameEvent ev;
            ev.type = msgType;
            if (t == Protocol::MessageType::PLAYER_HIT)
                packet >> ev.p1 >> ev.p2 >> ev.p3;
            else if (t == Protocol::MessageType::PLAYER_RESPAWN)
                packet >> ev.p1 >> ev.x >> ev.y;
            else if (t == Protocol::MessageType::GAME_OVER)
                packet >> ev.p1;

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
