// Este proyecto ha sido desarrollado con ayuda de inteligencia artificial.

#include <SFML/Graphics.hpp>
#include <iostream>
#include <memory>
#include "Core/Config.hpp"
#include "Network/LauncherClient.hpp"
#include "UI/LoginScreen.hpp"
#include "UI/MatchmakingScreen.hpp"
#include "Game/GameScreen.hpp"

// Configuracion de la ventana 
static constexpr unsigned int WINDOW_WIDTH  = 650;
static constexpr unsigned int WINDOW_HEIGHT = 480;
static constexpr unsigned int FRAMERATE_LIMIT = 60;

// Color de fondo de la aplicacion
static constexpr sf::Color BACKGROUND_COLOR{ 18, 18, 28 };

// Estados de la aplicacion (maquina de estados principal)
enum class AppState
{
    Login, // Pantalla de autenticacion
    Matchmaking, // Pantalla de busqueda de partida
    Game, // Partida en curso
};

int main()
{
    sf::RenderWindow window(
        sf::VideoMode({ WINDOW_WIDTH, WINDOW_HEIGHT }),
        "Shooter2D",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(FRAMERATE_LIMIT);

    sf::Font font;
    try
    {
        font = sf::Font("C:/Windows/Fonts/arial.ttf");
    }
    catch (const sf::Exception& e)
    {
        std::cerr << "Error cargando fuente: " << e.what() << "\n";
        return -1;
    }

    // Cargar configuracion de conexion desde server_config.ini junto al .exe
    Config::instance().load("server_config.ini");

    // Verificar y actualizar el mapa antes de mostrar el login
    {
        const Config& cfg = Config::instance();
        LauncherClient launcher;
        if (!launcher.checkAndUpdate(cfg.serverIP, cfg.authPort, "mapa.txt"))
            std::cerr << "[LAUNCHER] Aviso: no se pudo verificar el mapa, se usara el local" << std::endl;
    }

    AppState appState = AppState::Login;
    std::string localUsername;
    int localRanking = 0;

    std::unique_ptr<LoginScreen> loginScreen = std::make_unique<LoginScreen>(font);
    std::unique_ptr<MatchmakingScreen> matchmakingScreen;
    std::unique_ptr<GameScreen>        gameScreen;

    while (window.isOpen())
    {
        // Gestion de eventos
        while (const std::optional<sf::Event> event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            switch (appState)
            {
                case AppState::Login:
                    loginScreen->handleEvent(*event);
                    break;
                case AppState::Matchmaking:
                    matchmakingScreen->handleEvent(*event);
                    break;
                case AppState::Game:
                    break;
            }
        }

        // Actualizacion
        switch (appState)
        {
            case AppState::Login:
            {
                loginScreen->update();

                if (loginScreen->isComplete())
                {
                    const LoggedUser& user = loginScreen->getLoggedUser();
                    localUsername = user.username;
                    localRanking = user.ranking;

                    std::cout << "[CLIENT] Autenticado: " << localUsername << " | Ranking: " << localRanking << std::endl;

                    matchmakingScreen = std::make_unique<MatchmakingScreen>(font, localUsername, localRanking);
                    loginScreen.reset();

                    appState = AppState::Matchmaking;
                }
                break;
            }

            case AppState::Matchmaking:
            {
                matchmakingScreen->update();

                if (matchmakingScreen->isMatchFound())
                {
                    const MatchFound& match = matchmakingScreen->getMatchResult();
                    std::cout << "[CLIENT] Partida encontrada! Rival: " << match.opponentUsername << " | Servidor: " << match.gameServerIP  << ":" << match.gameServerPort << std::endl;

                    gameScreen = std::make_unique<GameScreen>(font, localUsername, localRanking, match);
                    matchmakingScreen.reset();

                    appState = AppState::Game;
                }
                break;
            }

            case AppState::Game:
                gameScreen->update(window.hasFocus());
                break;
        }

        // Render 
        window.clear(BACKGROUND_COLOR);

        switch (appState)
        {
            case AppState::Login:
                loginScreen->draw(window);
                break;
            case AppState::Matchmaking:
                matchmakingScreen->draw(window);
                break;
            case AppState::Game:
                gameScreen->draw(window);
                break;
        }

        window.display();
    }

    return 0;
}
