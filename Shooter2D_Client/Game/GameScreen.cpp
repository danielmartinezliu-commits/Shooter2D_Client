#include "GameScreen.hpp"
#include "../Network/Protocol.hpp"
#include <functional>
#include <iostream>
#include <optional>

namespace
{
    constexpr sf::Color LOCAL_COLOR { 50,  100, 220 };
    constexpr sf::Color REMOTE_COLOR{ 220,  50,  50 };
}

GameScreen::GameScreen(const sf::Font& font, const std::string& localUsername,
                       int localRanking, const MatchFound& match)
    : m_hudText(font, "", FONT_HUD_TOP)
    , m_localHealthText(font, "", FONT_HUD_HEALTH)
    , m_remoteHealthText(font, "", FONT_HUD_HEALTH)
    , m_localPlayerIndex(match.localPlayerIndex)
    , m_localName(localUsername)
    , m_remoteName(match.opponentUsername)
{
    if (!m_map.loadFromFile("mapa.txt"))
        std::cerr << "[GAME] No se pudo cargar mapa.txt\n";

    Player::loadTextures("assets/");

    const int remoteIndex = 1 - match.localPlayerIndex;
    m_local  = std::make_unique<Player>(m_map.spawnPoint(match.localPlayerIndex), LOCAL_COLOR,
                                        match.localPlayerIndex);
    m_remote = std::make_unique<Player>(m_map.spawnPoint(remoteIndex), REMOTE_COLOR,
                                        static_cast<uint8_t>(remoteIndex));

    if (!m_gameClient.connect(match.gameServerIP, match.gameServerPort,
                               localUsername, match.localPlayerIndex, match.isCompetitive))
        std::cerr << "[GAME] Sin conexion al servidor de juego\n";

    const std::string mode = match.isCompetitive ? "Competitivo" : "Amistoso";
    m_hudText.setString(
        localUsername + " (" + std::to_string(localRanking) + ")"
        + "  vs  "
        + match.opponentUsername + " (" + std::to_string(match.opponentRanking) + ")"
        + "  |  " + mode);
    m_hudText.setFillColor(sf::Color(210, 210, 210));
    m_hudText.setPosition({ HUD_TOP_X, HUD_TOP_Y });

    m_localHealthText.setFillColor(LOCAL_COLOR);
    m_localHealthText.setPosition({ HUD_LOCAL_X, HUD_LOCAL_Y });
    m_remoteHealthText.setFillColor(REMOTE_COLOR);
    m_remoteHealthText.setPosition({ HUD_REMOTE_X, HUD_REMOTE_Y });

    updateHealthHud();
}

// ================================================================
// update() — bucle principal de juego, un frame.
//
// Orden de operaciones (IMPORTANTE — no cambiar):
//   1. PREDICCION   : aplicar input localmente ANTES de enviar al servidor.
//   2. Red TX       : enviar PLAYER_INPUT al servidor.
//   3. INTERPOLACION: avanzar la visualizacion del rival.
//   4. Red RX       : drenar paquetes UDP entrantes.
//   5. RECONCILIACION: corregir prediccion si el servidor discrepa.
//   6. Actualizaciones del rival: añadir snapshots al buffer.
//   7. Eventos criticos: HIT, RESPAWN, GAME_OVER.
// ================================================================
void GameScreen::update(bool windowFocused)
{
    if (m_gameOver) return;

    const float dt = m_clock.restart().asSeconds();

    // ------------------------------------------------------------------
    // PASO 1+2 — PREDICCION DEL CLIENTE + envio de PLAYER_INPUT
    //
    // Aplicamos el input del teclado inmediatamente (sin esperar al
    // servidor) para que el jugador vea su movimiento en este mismo
    // frame. A continuacion enviamos el input al servidor para que
    // pueda validar y enviar PLAYER_STATE si hay divergencia.
    // ------------------------------------------------------------------
    if (windowFocused)
    {
        const InputFlags flags    = m_local->sampleInput();
        const bool       tryShoot = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
        const uint16_t   seq      = m_local->applyInput(flags, dt, m_map, tryShoot);
        m_gameClient.sendInput(seq, flags, dt);

        // Disparo: separado del sistema de prediccion de movimiento.
        // applyInput llama a fire() internamente si tryShoot+cooldown OK.
        if (m_local->justFired())
            m_gameClient.sendBullet(
                m_local->lastBulletPos().x,
                m_local->lastBulletPos().y,
                m_local->isFacingRight());

        // Burla: cosmetica, sin prediccion ni reconciliacion. isTaunting()
        // evita reenviar el mensaje en bucle mientras la animacion esta activa.
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::T) && !m_local->isTaunting())
        {
            m_local->playTaunt();
            m_gameClient.sendTaunt();
        }
    }
    else
    {
        // Ventana sin foco: input vacio, tryShoot=false (default).
        // sf::Keyboard::isKeyPressed es estado global del SO, asi que
        // sin este parametro otro cliente en la misma maquina dispara
        // cuando este cliente pulsa Space aunque no tenga el foco.
        const uint16_t seq = m_local->applyInput({}, dt, m_map);
        m_gameClient.sendInput(seq, {}, dt);
    }

    // Avanzar animaciones (debe ir antes del dibujado, en update)
    m_local->tickAnimation(dt);
    m_remote->tickAnimation(dt);

    // Enviar PING al servidor cada 2s para mantener el heartbeat.
    // El servidor usa estos paquetes para detectar desconexiones.
    m_gameClient.tickPing(dt);

    // ------------------------------------------------------------------
    // PASO 3 — INTERPOLACION DEL JUGADOR REMOTO
    //
    // update() interpola la posicion del rival usando el buffer de
    // snapshots: renderiza donde estaba hace INTERP_DELAY_S segundos.
    // Se llama ANTES de receiveAll() para usar el buffer del frame
    // anterior, garantizando siempre datos anteriores al instante actual.
    // ------------------------------------------------------------------
    m_remote->update(dt, m_map);

    // ------------------------------------------------------------------
    // PASO 4 — Recepcion de paquetes UDP
    // ------------------------------------------------------------------
    m_gameClient.receiveAll();

    // ------------------------------------------------------------------
    // PASO 5 — RECONCILIACION
    //
    // Si el servidor envia PLAYER_STATE con su posicion autoritativa
    // y esta difiere de nuestra prediccion, Player::reconcile() corrige
    // el estado actual y re-simula todos los inputs no confirmados.
    //
    // En condiciones normales (LAN, fisica determinista) el error es
    // cero o subpixel y no se produce ninguna correccion visible.
    // ------------------------------------------------------------------
    while (const std::optional<PlayerStateUpdate> state = m_gameClient.pollStateUpdate())
    {
        // Solo reconciliamos nuestro propio jugador
        if (state->playerIndex != m_localPlayerIndex) continue;

        m_local->reconcile(
            state->ackSeq,
            { state->x, state->y },
            state->vy,
            state->grounded,
            m_map);
    }

    // ------------------------------------------------------------------
    // PASO 6 — Actualizaciones del rival
    //
    // PLAYER_POS relay del servidor: anade el snapshot al buffer de
    // interpolacion del jugador remoto (Sistema 4).
    // PLAYER_SHOOT relay: crea la bala visual del rival.
    // ------------------------------------------------------------------
    while (const std::optional<RemoteUpdate> upd = m_gameClient.poll())
    {
        if (upd->playerIndex == m_localPlayerIndex) continue;

        if (upd->type == static_cast<uint8_t>(Protocol::MessageType::PLAYER_POS))
        {
            // Añadir snapshot al buffer de interpolacion en vez de actualizar
            // la posicion directamente. La interpolacion en update() suaviza
            // el movimiento aunque los paquetes lleguen con jitter.
            m_remote->addSnapshot({ upd->x, upd->y }, upd->facingRight);
        }
        else if (upd->type == static_cast<uint8_t>(Protocol::MessageType::PLAYER_SHOOT))
        {
            const float vx = upd->facingRight ? Player::BULLET_SPEED : -Player::BULLET_SPEED;
            m_remote->bullets().push_back({ {upd->x, upd->y}, {vx, 0.f} });
        }
    }

    // ------------------------------------------------------------------
    // PASO 7 — Eventos criticos (HIT / RESPAWN / GAME_OVER)
    // ------------------------------------------------------------------
    handleGameEvents();
}

void GameScreen::handleGameEvents()
{
    while (std::optional<GameEvent> ev = m_gameClient.pollEvent())
    {
        const Protocol::MessageType type = static_cast<Protocol::MessageType>(ev->type);

        if (type == Protocol::MessageType::PLAYER_HIT)
        {
            m_hp   [ev->p1] = ev->p2;
            m_lives[ev->p1] = ev->p3;
            updateHealthHud();

            // Activar animacion de dano en el jugador golpeado
            if (ev->p1 == m_localPlayerIndex)
                m_local->playDamageAnim();
            else
                m_remote->playDamageAnim();
        }
        else if (type == Protocol::MessageType::PLAYER_RESPAWN)
        {
            if (ev->p1 == m_localPlayerIndex)
                m_local->respawn({ ev->x, ev->y });
            else
                m_remote->respawn({ ev->x, ev->y });
        }
        else if (type == Protocol::MessageType::GAME_OVER)
        {
            m_gameOver = true;
            const bool localLost = (ev->p1 == m_localPlayerIndex);
            m_hudText.setString(localLost ? "DERROTA" : "VICTORIA");
            m_hudText.setFillColor(localLost ? REMOTE_COLOR : LOCAL_COLOR);
            m_hudText.setCharacterSize(FONT_GAME_OVER);
            m_hudText.setPosition({
                WINDOW_W / 2.f - GAME_OVER_OFFSET_X,
                WINDOW_H / 2.f - GAME_OVER_OFFSET_Y });
        }
        else if (type == Protocol::MessageType::PLAYER_TAUNT)
        {
            // El relay del servidor solo llega del rival (el local ya se
            // reprodujo localmente al pulsar la tecla), pero comprobamos
            // el indice por si el orden de los paquetes UDP varia.
            if (ev->p1 != m_localPlayerIndex)
                m_remote->playTaunt();
        }
        else if (type == Protocol::MessageType::GAME_ABANDON)
        {
            // El rival dejo de enviar paquetes y el servidor lo detecto.
            m_gameOver = true;
            m_hudText.setString("RIVAL DESCONECTADO");
            m_hudText.setFillColor(sf::Color(200, 200, 200));
            m_hudText.setCharacterSize(FONT_GAME_OVER);
            m_hudText.setPosition({
                WINDOW_W / 2.f - GAME_OVER_OFFSET_X,
                WINDOW_H / 2.f - GAME_OVER_OFFSET_Y });
        }
    }
}

void GameScreen::updateHealthHud()
{
    const std::function<std::string(const std::string&, uint8_t, uint8_t)> makeStr = [](const std::string& name, uint8_t lives, uint8_t hp) -> std::string
    {
        return name + "  Vidas: " + std::to_string(lives) + "  HP: " + std::to_string(hp);
    };
    m_localHealthText.setString (makeStr(m_localName,  m_lives[m_localPlayerIndex],     m_hp[m_localPlayerIndex]));
    m_remoteHealthText.setString(makeStr(m_remoteName, m_lives[1 - m_localPlayerIndex], m_hp[1 - m_localPlayerIndex]));
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
