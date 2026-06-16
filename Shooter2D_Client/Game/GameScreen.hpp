#pragma once
#include <SFML/Graphics.hpp>
#include <array>
#include <memory>
#include <string>
#include "Map.hpp"
#include "Player.hpp"
#include "GameClient.hpp"
#include "../Network/MatchmakingClient.hpp"

class GameScreen
{
public:
    // @brief Construye la pantalla de juego y conecta al servidor.
    GameScreen(const sf::Font& font, const std::string& localUsername,
               int localRanking, const MatchFound& match);

    // @brief Actualiza un frame: prediccion, red, reconciliacion, interpolacion.
    void update(bool windowFocused);

    // @brief Dibuja mapa, jugadores y HUD.
    void draw(sf::RenderWindow& window) const;

private:
    // @brief Procesa eventos criticos (HIT, RESPAWN, GAME_OVER).
    void handleGameEvents();

    // @brief Actualiza los textos de HP/vidas del HUD inferior.
    void updateHealthHud();

    // HUD
    static constexpr float        HUD_TOP_X         =   6.f;
    static constexpr float        HUD_TOP_Y         =   4.f;
    static constexpr float        HUD_LOCAL_X       =   6.f;
    static constexpr float        HUD_LOCAL_Y       = 452.f;
    static constexpr float        HUD_REMOTE_X      = 370.f;
    static constexpr float        HUD_REMOTE_Y      = 452.f;
    static constexpr unsigned int FONT_HUD_TOP      =  13;
    static constexpr unsigned int FONT_HUD_HEALTH   =  15;
    static constexpr unsigned int FONT_GAME_OVER    =  36;
    static constexpr float        WINDOW_W          = 650.f;
    static constexpr float        WINDOW_H          = 480.f;
    static constexpr float        GAME_OVER_OFFSET_X=  80.f;
    static constexpr float        GAME_OVER_OFFSET_Y=  18.f;

    // Entidades de juego
    Map                    m_map;
    std::unique_ptr<Player> m_local;
    std::unique_ptr<Player> m_remote;
    GameClient             m_gameClient;
    sf::Clock              m_clock;

    // HUD
    sf::Text m_hudText;
    sf::Text m_localHealthText;
    sf::Text m_remoteHealthText;

    std::array<uint8_t, 2> m_hp    = { 5, 5 };
    std::array<uint8_t, 2> m_lives = { 3, 3 };

    std::string m_localName;
    std::string m_remoteName;
    uint8_t     m_localPlayerIndex = 0;
    bool        m_gameOver         = false;
    // Nota: m_sendTimer eliminado — con prediccion se envia input CADA FRAME
};
