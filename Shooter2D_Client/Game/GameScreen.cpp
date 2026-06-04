#include "GameScreen.hpp"
#include "../Network/Protocol.hpp"
#include <functional>
#include <iostream>
#include <optional>

namespace
{
    constexpr sf::Color LOCAL_COLOR { 50,  100, 220 }; // Color del jugador local (azul)
    constexpr sf::Color REMOTE_COLOR{ 220,  50,  50 }; // Color del jugador rival (rojo)
}

GameScreen::GameScreen(const sf::Font& font, const std::string& localUsername, int localRanking, const MatchFound& match)
    : m_hudText(font, "", FONT_HUD_TOP)
    , m_localHealthText(font, "", FONT_HUD_HEALTH)
    , m_remoteHealthText(font, "", FONT_HUD_HEALTH)
    , m_localPlayerIndex(match.localPlayerIndex)
    , m_localName(localUsername)
    , m_remoteName(match.opponentUsername)
{
    if (!m_map.loadFromFile("mapa.txt"))
        std::cerr << "[GAME] No se pudo cargar mapa.txt\n";

    const int remoteIndex = 1 - match.localPlayerIndex;
    m_local = std::make_unique<Player>(m_map.spawnPoint(match.localPlayerIndex), LOCAL_COLOR);
    m_remote = std::make_unique<Player>(m_map.spawnPoint(remoteIndex), REMOTE_COLOR);

    if (!m_gameClient.connect(match.gameServerIP, match.gameServerPort,
                               localUsername, match.localPlayerIndex))
    {
        std::cerr << "[GAME] Sin conexion al servidor de juego\n";
    }

    // HUD superior: nombres, rankings y modo de partida
    const std::string mode = match.isCompetitive ? "Competitivo" : "Amistoso";
    m_hudText.setString(
        localUsername + " (" + std::to_string(localRanking) + ")" + "  vs  "
        + match.opponentUsername + " (" + std::to_string(match.opponentRanking) + ")" + "  |  " + mode);
    m_hudText.setFillColor(sf::Color(210, 210, 210));
    m_hudText.setPosition({ HUD_TOP_X, HUD_TOP_Y });

    // HUD inferior: salud de cada jugador
    m_localHealthText.setFillColor(LOCAL_COLOR);
    m_localHealthText.setPosition({ HUD_LOCAL_X, HUD_LOCAL_Y });

    m_remoteHealthText.setFillColor(REMOTE_COLOR);
    m_remoteHealthText.setPosition({ HUD_REMOTE_X, HUD_REMOTE_Y });

    updateHealthHud();
}

void GameScreen::update(bool windowFocused)
{
    if (m_gameOver) return;

    const float dt = m_clock.restart().asSeconds();

    if (windowFocused)
    {
        m_local->processInput();

        // Notificar disparo al servidor en el mismo frame en que ocurre
        if (m_local->justFired())
            m_gameClient.sendBullet( m_local->lastBulletPos().x, m_local->lastBulletPos().y, m_local->isFacingRight());
    }
    else
    {
        m_local->stopHorizontal();
    }

    m_local->update(dt, m_map);
    m_remote->update(dt, m_map, false); // El remoto no aplica fisica propia

    // Enviar posicion al servidor a SEND_INTERVAL para reducir trafico UDP
    m_sendTimer += dt;
    if (m_sendTimer >= SEND_INTERVAL)
    {
        m_sendTimer = 0.f;
        m_gameClient.sendPosition(
            m_local->position().x,
            m_local->position().y,
            m_local->isFacingRight());
    }

    // Drenar todos los paquetes UDP recibidos este frame
    m_gameClient.receiveAll();

    // Aplicar actualizaciones no fiables del rival (posicion y disparo)
    while (std::optional<RemoteUpdate> upd = m_gameClient.poll())
    {
        if (upd->playerIndex == m_localPlayerIndex) continue;

        if (upd->type == static_cast<uint8_t>(Protocol::MessageType::PLAYER_POS))
            m_remote->setRemoteState({ upd->x, upd->y }, upd->facingRight);
        else if (upd->type == static_cast<uint8_t>(Protocol::MessageType::PLAYER_SHOOT))
        {
            const float vx = upd->facingRight ? Player::BULLET_SPEED : -Player::BULLET_SPEED;
            m_remote->bullets().push_back({ {upd->x, upd->y}, {vx, 0.f} });
        }
    }

    handleGameEvents();
}

void GameScreen::handleGameEvents()
{
    while (std::optional<GameEvent> ev = m_gameClient.pollEvent())
    {
        const Protocol::MessageType type = static_cast<Protocol::MessageType>(ev->type);

        if (type == Protocol::MessageType::PLAYER_HIT)
        {
            // p1=victimIndex, p2=newHp, p3=newLives
            m_hp   [ev->p1] = ev->p2;
            m_lives[ev->p1] = ev->p3;
            updateHealthHud();
        }
        else if (type == Protocol::MessageType::PLAYER_RESPAWN)
        {
            // p1=playerIndex, x/y=posicion de reaparicion
            if (ev->p1 == m_localPlayerIndex)
                m_local->respawn({ ev->x, ev->y });
            else
                m_remote->respawn({ ev->x, ev->y });
        }
        else if (type == Protocol::MessageType::GAME_OVER)
        {
            // p1=loserIndex
            m_gameOver = true;
            const bool localLost = (ev->p1 == m_localPlayerIndex);
            m_hudText.setString(localLost ? "DERROTA" : "VICTORIA");
            m_hudText.setFillColor(localLost ? REMOTE_COLOR : LOCAL_COLOR);
            m_hudText.setCharacterSize(FONT_GAME_OVER);
            m_hudText.setPosition({ WINDOW_W / 2.f - GAME_OVER_OFFSET_X, WINDOW_H / 2.f - GAME_OVER_OFFSET_Y });
        }
    }
}

void GameScreen::updateHealthHud()
{
    const std::function<std::string(const std::string&, uint8_t, uint8_t)> makeStr = [](const std::string& name, uint8_t lives, uint8_t hp) -> std::string
    {
        return name + "  Vidas: " + std::to_string(lives) + "  HP: " + std::to_string(hp);
    };

    m_localHealthText.setString(makeStr(m_localName, m_lives[m_localPlayerIndex], m_hp [m_localPlayerIndex]));

    m_remoteHealthText.setString(makeStr(m_remoteName, m_lives[1 - m_localPlayerIndex], m_hp [1 - m_localPlayerIndex]));
}

void GameScreen::draw(sf::RenderWindow& window) const
{
    m_map.draw(window);
    m_remote->draw(window);
    m_local->draw(window);
    window.draw(m_hudText);
    window.draw(m_localHealthText);
    window.draw(m_remoteHealthText);
}
