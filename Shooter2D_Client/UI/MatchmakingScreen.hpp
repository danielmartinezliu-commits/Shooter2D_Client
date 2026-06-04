#pragma once
#include <SFML/Graphics.hpp>
#include <future>
#include <string>
#include "UIComponents.hpp"
#include "../Network/MatchmakingClient.hpp"

/// Estados posibles de la pantalla de matchmaking.
enum class MatchmakingState
{
    Idle,      ///< Mostrando botones de seleccion de modo
    Searching, ///< Buscando pareja (future pendiente)
    Found,     ///< Pareja encontrada, transicionando a la partida
    Error,     ///< Error o timeout; permite reintentar
};

// ============================================================
// PANTALLA DE MATCHMAKING
// ============================================================
// Permite al jugador elegir entre partida amistosa y competitiva.
// Lanza la busqueda en un hilo de fondo via MatchmakingClient y
// muestra el estado de la busqueda hasta encontrar rival o cancelar.
// ============================================================

class MatchmakingScreen
{
public:
    /**
     * @brief Construye la pantalla de matchmaking con los datos del jugador autenticado.
     * @param font     Fuente a usar en todos los elementos UI.
     * @param username Nombre del jugador que busca partida.
     * @param ranking  Ranking actual del jugador.
     */
    MatchmakingScreen(const sf::Font& font, const std::string& username, int ranking);

    /**
     * @brief Procesa un evento SFML (clicks para seleccionar modo o cancelar).
     * @param event Evento SFML a gestionar.
     */
    void handleEvent(const sf::Event& event);

    /**
     * @brief Actualiza el estado: comprueba el future de matchmaking cada frame.
     */
    void update();

    /**
     * @brief Dibuja los elementos UI segun el estado actual.
     * @param window Ventana de destino.
     */
    void draw(sf::RenderWindow& window) const;

    /**
     * @brief Indica si se ha encontrado pareja y el juego puede comenzar.
     * @return true si el estado es Found.
     */
    bool isMatchFound() const { return m_state == MatchmakingState::Found; }

    /**
     * @brief Devuelve los datos de la partida encontrada.
     *        Solo valido si isMatchFound() retorna true.
     * @return Referencia constante al MatchFound con datos del rival y servidor.
     */
    const MatchFound& getMatchResult() const { return m_matchResult; }

private:
    /**
     * @brief Inicia la busqueda de partida en el modo indicado.
     * @param isCompetitive true para cola competitiva, false para amistosa.
     */
    void startSearch(bool isCompetitive);

    /**
     * @brief Cancela la busqueda en curso notificando al MatchmakingClient.
     */
    void cancelSearch();

    /**
     * @brief Procesa el MatchFound recibido del future y actualiza el estado.
     * @param result Resultado del matchmaking (exito o fallo).
     */
    void onResult(MatchFound result);

    /**
     * @brief Muestra un mensaje de estado con el color indicado.
     * @param msg   Texto a mostrar.
     * @param color Color del texto.
     */
    void setStatus(const std::string& msg, sf::Color color);

    // --- Elementos UI ---
    sf::Text m_title;
    sf::Text m_playerInfo;
    Button   m_btnFriendly;
    Button   m_btnCompetitive;
    Button   m_btnCancel;
    sf::Text m_searchingText;
    sf::Text m_resultText;
    sf::Text m_statusText;

    // --- Estado interno ---
    MatchmakingState        m_state = MatchmakingState::Idle;
    MatchmakingClient       m_client;
    std::future<MatchFound> m_pendingResult;
    MatchFound              m_matchResult;

    std::string m_username; ///< Nombre del jugador local
    int         m_ranking;  ///< Ranking del jugador local
};
