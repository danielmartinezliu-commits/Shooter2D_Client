#pragma once
#include <SFML/Graphics.hpp>
#include <future>
#include <string>
#include "UIComponents.hpp"
#include "../Network/AuthClient.hpp"

enum class LoginState
{
    Idle, // Esperando input del usuario
    Waiting, // Peticion TCP en curso (std::future pendiente)
    Success, // Login o registro completado con exito
};

// Datos del usuario tras autenticarse correctamente.
struct LoggedUser
{
    std::string username;
    int ranking = 0;
};

class LoginScreen
{
public:
    //Construye la pantalla de login inicializando todos los widgets UI.
    explicit LoginScreen(const sf::Font& font);

    // Procesa un evento SFML (clicks de raton, teclado).
    void handleEvent(const sf::Event& event);

    // Actualiza el estado de la pantalla: comprueba el future de autenticacion.
    void update();

    // Dibuja todos los elementos de la pantalla de login.
    void draw(sf::RenderWindow& window) const;

    // Indica si el proceso de login/registro ha terminado con exito.
    bool isComplete() const { return m_state == LoginState::Success; }

    // Devuelve los datos del usuario autenticado.
    const LoggedUser& getLoggedUser() const { return m_loggedUser; }

private:
    // Valida campos y lanza la peticion de login via AuthClient.
    void submitLogin();

    // Valida campos y lanza la peticion de registro via AuthClient.
    void submitRegister();

    // Procesa el AuthResult recibido del future y actualiza el estado.
    void onAuthResult(AuthResult result);

    // Cambia al tab de login, limpiando campos y estado.
    void switchToLogin();

    // Cambia al tab de registro, limpiando campos y estado.
    void switchToRegister();

    // Muestra un mensaje de estado con el color indicado.
    void setStatus(const std::string& msg, sf::Color color);

    // Tabs
    Button m_tabLogin;
    Button m_tabRegister;

    // Campos de entrada
    TextInput m_inputUsername;
    TextInput m_inputPassword;

    // Boton de accion
    Button m_btnSubmit;

    // Textos informativos
    sf::Text m_title;
    sf::Text m_labelUsername;
    sf::Text m_labelPassword;
    sf::Text m_statusText;

    // Estado interno
    bool m_onRegisterTab = false; // true si el tab activo es Registro
    LoginState m_state = LoginState::Idle;
    AuthClient m_client; // Cliente de autenticacion TCP
    std::future<AuthResult> m_pendingResult; // Future de la operacion en curso
    LoggedUser m_loggedUser; // Datos del usuario autenticado
    const sf::Font& m_font; // Referencia a la fuente global
};
