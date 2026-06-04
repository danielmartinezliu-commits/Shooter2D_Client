#include "LoginScreen.hpp"
#include "../Network/Protocol.hpp"
#include "../Core/Config.hpp"
#include <chrono>

// UI hecho completamente con IA

// --- Posiciones de los elementos UI (ventana 650x480) ---
static constexpr float CENTER_X   = 325.f;
static constexpr float TITLE_Y    = 55.f;
static constexpr float TAB_Y      = 110.f;
static constexpr float FIELD_X    = 175.f;
static constexpr float FIELD_W    = 300.f;
static constexpr float FIELD_H    =  40.f;
static constexpr float TAB_W      = 145.f;
static constexpr float TAB_H      =  38.f;
static constexpr float TAB_GAP    = 155.f; ///< Separacion entre el inicio del tab 1 y el tab 2
static constexpr float USERNAME_Y = 200.f;
static constexpr float PASSWORD_Y = 270.f;
static constexpr float SUBMIT_Y   = 340.f;
static constexpr float SUBMIT_H   =  44.f;
static constexpr float STATUS_Y   = 405.f;
static constexpr float LABEL_OFFSET_Y = 20.f; ///< Desplazamiento vertical del label sobre el campo

// --- Tamanos de fuente ---
static constexpr unsigned int FONT_TITLE  = 32;
static constexpr unsigned int FONT_LABEL  = 14;
static constexpr unsigned int FONT_STATUS = 15;

// --- Colores de estado ---
static constexpr sf::Color COL_LABEL   { 180, 180, 200 };
static constexpr sf::Color COL_ERROR   { 220,  80,  80 };
static constexpr sf::Color COL_WAITING { 220, 200,  80 };
static constexpr sf::Color COL_SUCCESS {  80, 220, 100 };

LoginScreen::LoginScreen(const sf::Font& font)
    : m_tabLogin    (font, "Login",     { FIELD_X,          TAB_Y }, { TAB_W, TAB_H })
    , m_tabRegister (font, "Registro",  { FIELD_X + TAB_GAP, TAB_Y }, { TAB_W, TAB_H })
    , m_inputUsername(font, { FIELD_X, USERNAME_Y }, { FIELD_W, FIELD_H }, "Usuario")
    , m_inputPassword(font, { FIELD_X, PASSWORD_Y }, { FIELD_W, FIELD_H }, "Contrasena", true)
    , m_btnSubmit   (font, "Entrar",    { FIELD_X, SUBMIT_Y }, { FIELD_W, SUBMIT_H })
    , m_title       (font, "Shooter 2D", FONT_TITLE)
    , m_labelUsername(font, "Usuario:", FONT_LABEL)
    , m_labelPassword(font, "Contrasena:", FONT_LABEL)
    , m_statusText  (font, "", FONT_STATUS)
    , m_font(font)
{
    m_title.setFillColor(sf::Color::White);
    const sf::FloatRect titleBounds = m_title.getLocalBounds();
    m_title.setOrigin({ titleBounds.position.x + titleBounds.size.x / 2.f,
                        titleBounds.position.y + titleBounds.size.y / 2.f });
    m_title.setPosition({ CENTER_X, TITLE_Y });

    m_labelUsername.setFillColor(COL_LABEL);
    m_labelUsername.setPosition({ FIELD_X, USERNAME_Y - LABEL_OFFSET_Y });

    m_labelPassword.setFillColor(COL_LABEL);
    m_labelPassword.setPosition({ FIELD_X, PASSWORD_Y - LABEL_OFFSET_Y });

    m_statusText.setPosition({ FIELD_X, STATUS_Y });

    m_tabLogin.setActive(true);
}

void LoginScreen::handleEvent(const sf::Event& event)
{
    if (m_state == LoginState::Waiting) return;

    m_inputUsername.handleEvent(event);
    m_inputPassword.handleEvent(event);

    if (const sf::Event::MouseButtonReleased* mb = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if (mb->button != sf::Mouse::Button::Left) return;

        const sf::Vector2f mouse = static_cast<sf::Vector2f>(mb->position);

        if (m_tabLogin.contains(mouse))    { switchToLogin();    return; }
        if (m_tabRegister.contains(mouse)) { switchToRegister(); return; }

        // Gestion de foco: solo el campo pulsado queda activo
        m_inputUsername.setFocused(m_inputUsername.contains(mouse));
        m_inputPassword.setFocused(m_inputPassword.contains(mouse));

        if (m_btnSubmit.contains(mouse))
        {
            if (m_onRegisterTab) submitRegister();
            else                 submitLogin();
        }
    }
}

void LoginScreen::update()
{
    if (m_state != LoginState::Waiting || !m_pendingResult.valid()) return;

    // wait_for(0) no bloquea: retorna ready si el future ya tiene resultado.
    if (m_pendingResult.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
        onAuthResult(m_pendingResult.get());
}

void LoginScreen::draw(sf::RenderWindow& window) const
{
    window.draw(m_title);
    m_tabLogin.draw(window);
    m_tabRegister.draw(window);
    window.draw(m_labelUsername);
    m_inputUsername.draw(window);
    window.draw(m_labelPassword);
    m_inputPassword.draw(window);
    m_btnSubmit.draw(window);
    window.draw(m_statusText);
}

void LoginScreen::submitLogin()
{
    const std::string& user = m_inputUsername.getText();
    const std::string& pass = m_inputPassword.getText();

    if (user.empty() || pass.empty())
    {
        setStatus("Rellena todos los campos", COL_ERROR);
        return;
    }

    m_state = LoginState::Waiting;
    m_btnSubmit.setEnabled(false);
    setStatus("Conectando...", COL_WAITING);

    const Config& cfg = Config::instance();
    m_pendingResult = m_client.login(cfg.serverIP, cfg.authPort, user, pass);
}

void LoginScreen::submitRegister()
{
    const std::string& user = m_inputUsername.getText();
    const std::string& pass = m_inputPassword.getText();

    if (user.empty() || pass.empty())
    {
        setStatus("Rellena todos los campos", COL_ERROR);
        return;
    }

    m_state = LoginState::Waiting;
    m_btnSubmit.setEnabled(false);
    setStatus("Registrando...", COL_WAITING);

    const Config& cfg = Config::instance();
    m_pendingResult = m_client.registerUser(cfg.serverIP, cfg.authPort, user, pass);
}

void LoginScreen::onAuthResult(AuthResult result)
{
    m_btnSubmit.setEnabled(true);

    if (result.success)
    {
        m_state      = LoginState::Success;
        m_loggedUser = { m_inputUsername.getText(), result.ranking };
        setStatus("Bienvenido! Ranking: " + std::to_string(result.ranking), COL_SUCCESS);
    }
    else
    {
        m_state = LoginState::Idle;
        setStatus(result.message, COL_ERROR);
    }
}

void LoginScreen::switchToLogin()
{
    if (!m_onRegisterTab) return;
    m_onRegisterTab = false;
    m_btnSubmit.setLabel("Entrar");
    m_tabLogin.setActive(true);
    m_tabRegister.setActive(false);
    m_inputUsername.clear();
    m_inputPassword.clear();
    setStatus("", sf::Color::Transparent);
}

void LoginScreen::switchToRegister()
{
    if (m_onRegisterTab) return;
    m_onRegisterTab = true;
    m_btnSubmit.setLabel("Registrarse");
    m_tabLogin.setActive(false);
    m_tabRegister.setActive(true);
    m_inputUsername.clear();
    m_inputPassword.clear();
    setStatus("", sf::Color::Transparent);
}

void LoginScreen::setStatus(const std::string& msg, sf::Color color)
{
    m_statusText.setString(msg);
    m_statusText.setFillColor(color);
}
