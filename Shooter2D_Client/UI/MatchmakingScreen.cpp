#include "MatchmakingScreen.hpp"
#include "../Network/Protocol.hpp"
#include "../Core/Config.hpp"
#include <chrono>

// Posiciones de los elementos UI (ventana 650x480)
static constexpr float CENTER_X        = 325.f;
static constexpr float TITLE_Y         =  50.f;
static constexpr float PLAYER_INFO_X   = 130.f;
static constexpr float PLAYER_INFO_Y   = 110.f;
static constexpr float BTN_Y           = 190.f;
static constexpr float BTN_FRIENDLY_X  = 130.f;
static constexpr float BTN_COMPET_X    = 335.f;
static constexpr float BTN_W           = 185.f;
static constexpr float BTN_H           =  55.f;
static constexpr float BTN_CANCEL_X    = 200.f;
static constexpr float BTN_CANCEL_Y    = 320.f;
static constexpr float BTN_CANCEL_W    = 250.f;
static constexpr float BTN_CANCEL_H    =  44.f;
static constexpr float SEARCHING_X     = 130.f;
static constexpr float SEARCHING_Y     = 270.f;
static constexpr float RESULT_X        = 130.f;
static constexpr float RESULT_Y        = 220.f;
static constexpr float STATUS_X        = 130.f;
static constexpr float STATUS_Y        = 390.f;

// Tamanos de fuente
static constexpr unsigned int FONT_INFO      = 16;
static constexpr unsigned int FONT_SEARCHING = 16;
static constexpr unsigned int FONT_RESULT    = 17;
static constexpr unsigned int FONT_TITLE     = 30;
static constexpr unsigned int FONT_STATUS    = 15;

// Colores
static constexpr sf::Color COL_PLAYER_INFO{ 180, 180, 200 };
static constexpr sf::Color COL_SEARCHING  { 220, 200,  80 };
static constexpr sf::Color COL_RESULT     {  80, 220, 100 };
static constexpr sf::Color COL_ERROR      { 220,  80,  80 };

MatchmakingScreen::MatchmakingScreen(const sf::Font& font,
                                     const std::string& username, int ranking)
    : m_playerInfo  (font, "Jugador: " + username + "   Ranking: " + std::to_string(ranking),
                     FONT_INFO)
    , m_btnFriendly (font, "Partida Amistosa",  { BTN_FRIENDLY_X, BTN_Y }, { BTN_W, BTN_H })
    , m_btnCompetitive(font, "Competitivo",     { BTN_COMPET_X,   BTN_Y }, { BTN_W, BTN_H })
    , m_btnCancel   (font, "Cancelar busqueda", { BTN_CANCEL_X, BTN_CANCEL_Y },
                                                { BTN_CANCEL_W, BTN_CANCEL_H })
    , m_searchingText(font, "", FONT_SEARCHING)
    , m_resultText  (font, "", FONT_RESULT)
    , m_title       (font, "Matchmaking", FONT_TITLE)
    , m_statusText  (font, "", FONT_STATUS)
    , m_username(username)
    , m_ranking(ranking)
{
    m_title.setFillColor(sf::Color::White);
    const sf::FloatRect tb = m_title.getLocalBounds();
    m_title.setOrigin({ tb.position.x + tb.size.x / 2.f,
                        tb.position.y + tb.size.y / 2.f });
    m_title.setPosition({ CENTER_X, TITLE_Y });

    m_playerInfo.setFillColor(COL_PLAYER_INFO);
    m_playerInfo.setPosition({ PLAYER_INFO_X, PLAYER_INFO_Y });

    m_searchingText.setFillColor(COL_SEARCHING);
    m_searchingText.setPosition({ SEARCHING_X, SEARCHING_Y });

    m_resultText.setFillColor(COL_RESULT);
    m_resultText.setPosition({ RESULT_X, RESULT_Y });

    m_statusText.setFillColor(COL_ERROR);
    m_statusText.setPosition({ STATUS_X, STATUS_Y });
}

void MatchmakingScreen::handleEvent(const sf::Event& event)
{
    if (const sf::Event::MouseButtonReleased* mb = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if (mb->button != sf::Mouse::Button::Left) return;
        const sf::Vector2f mouse = static_cast<sf::Vector2f>(mb->position);

        switch (m_state)
        {
            case MatchmakingState::Idle:
                if (m_btnFriendly.contains(mouse))    startSearch(false);
                if (m_btnCompetitive.contains(mouse)) startSearch(true);
                break;

            case MatchmakingState::Searching:
                if (m_btnCancel.contains(mouse)) cancelSearch();
                break;

            default:
                break;
        }
    }
}

void MatchmakingScreen::update()
{
    if (m_state != MatchmakingState::Searching || !m_pendingResult.valid()) return;

    // wait_for(0) no bloquea: retorna ready si el future ya tiene resultado.
    if (m_pendingResult.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
        onResult(m_pendingResult.get());
}

void MatchmakingScreen::draw(sf::RenderWindow& window) const
{
    window.draw(m_title);
    window.draw(m_playerInfo);

    switch (m_state)
    {
        case MatchmakingState::Idle:
        case MatchmakingState::Error:
            m_btnFriendly.draw(window);
            m_btnCompetitive.draw(window);
            break;

        case MatchmakingState::Searching:
            window.draw(m_searchingText);
            m_btnCancel.draw(window);
            break;

        case MatchmakingState::Found:
            window.draw(m_resultText);
            break;
    }

    window.draw(m_statusText);
}

void MatchmakingScreen::startSearch(bool isCompetitive)
{
    m_state = MatchmakingState::Searching;
    m_btnFriendly.setEnabled(false);
    m_btnCompetitive.setEnabled(false);

    const std::string modeStr = isCompetitive ? "competitiva" : "amistosa";
    m_searchingText.setString("Buscando partida " + modeStr + "...\n(Max 2 minutos)");
    setStatus("", sf::Color::Transparent);

    const Config& cfg = Config::instance();
    m_pendingResult = m_client.join(cfg.serverIP, cfg.authPort, m_username, isCompetitive);
}

void MatchmakingScreen::cancelSearch()
{
    m_client.cancel();
    // El future se resolvera con success=false en el proximo update()
}

void MatchmakingScreen::onResult(MatchFound result)
{
    m_btnFriendly.setEnabled(true);
    m_btnCompetitive.setEnabled(true);

    if (result.success)
    {
        m_state       = MatchmakingState::Found;
        m_matchResult = result;

        const std::string mode = result.isCompetitive ? "Competitivo" : "Amistoso";
        m_resultText.setString(
            "Partida encontrada! [" + mode + "]\n"
            "Rival: " + result.opponentUsername +
            "  (Ranking: " + std::to_string(result.opponentRanking) + ")\n"
            "Servidor: " + result.gameServerIP +
            ":" + std::to_string(result.gameServerPort) +
            "\n\nConectando al servidor de juego...");
        setStatus("", sf::Color::Transparent);
    }
    else
    {
        // Error o cancelacion: volver a Idle para permitir reintentar
        m_state = MatchmakingState::Idle;
        setStatus(result.message, COL_ERROR);
    }
}

void MatchmakingScreen::setStatus(const std::string& msg, sf::Color color)
{
    m_statusText.setString(msg);
    m_statusText.setFillColor(color);
}
