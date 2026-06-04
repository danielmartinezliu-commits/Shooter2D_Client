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
    // Construye la pantalla de juego y conecta al servidor de juego.
    GameScreen(const sf::Font& font, const std::string& localUsername, int localRanking, const MatchFound& match);

    // Actualiza la logica de juego un frame: input, fisica, red y eventos.
    void update(bool windowFocused);

    // Dibuja el mapa, los jugadores y el HUD en la ventana.
    void draw(sf::RenderWindow& window) const;

private:
    // Procesa todos los eventos TCP fiables pendientes del GameClient(PLAYER_HIT, PLAYER_RESPAWN, GAME_OVER).
    void handleGameEvents();

    // Actualiza los textos del HUD inferior con el HP y vidas actuales.
    void updateHealthHud();

    // Constantes de HUD
    static constexpr float HUD_TOP_X = 6.f; // X del HUD superior
    static constexpr float HUD_TOP_Y = 4.f; // Y del HUD superior
    static constexpr float HUD_LOCAL_X = 6.f; // X del HUD de salud local
    static constexpr float HUD_LOCAL_Y = 452.f; // Y del HUD de salud local
    static constexpr float HUD_REMOTE_X = 370.f; // X del HUD de salud rival
    static constexpr float HUD_REMOTE_Y  = 452.f; // Y del HUD de salud rival
    static constexpr unsigned int FONT_HUD_TOP = 13; // Tamano fuente HUD superior
    static constexpr unsigned int FONT_HUD_HEALTH = 15; // Tamano fuente HUD de salud
    static constexpr unsigned int FONT_GAME_OVER  = 36; // Tamano fuente GAME OVER
    static constexpr float SEND_INTERVAL = 0.05f; // Intervalo de envio de posicion (segundos)
    static constexpr float WINDOW_W = 650.f; // Anchura de la ventana (px)
    static constexpr float WINDOW_H = 480.f; // Altura de la ventana (px)
    static constexpr float GAME_OVER_OFFSET_X = 80.f; // Offset X del texto GAME OVER
    static constexpr float GAME_OVER_OFFSET_Y = 18.f; // Offset Y del texto GAME OVER

    // Entidades de juego
    Map m_map;
    std::unique_ptr<Player> m_local;
    std::unique_ptr<Player> m_remote;
    GameClient m_gameClient;
    sf::Clock m_clock;

    // HUD
    sf::Text m_hudText;
    sf::Text m_localHealthText;
    sf::Text m_remoteHealthText;

    std::array<uint8_t, 2> m_hp = { 5, 5 }; // HP actual de cada jugador (indexado por playerIndex)
    std::array<uint8_t, 2> m_lives = { 3, 3 }; // Vidas de cada jugador

    // Informacion de la partida
    std::string m_localName;
    std::string m_remoteName;
    uint8_t m_localPlayerIndex = 0; // Indice de este cliente (0 o 1)
    float m_sendTimer= 0.f; // Acumulador para el intervalo de envio de posicion
    bool m_gameOver = false;
};
