#include "UIComponents.hpp"

// --- Constantes de estilo del boton ---
static constexpr unsigned int BTN_FONT_SIZE     = 18;
static constexpr float        BTN_OUTLINE_W     = 1.f;
static constexpr float        BTN_OUTLINE_W_FOCUSED = 2.f;

static constexpr sf::Color BTN_FILL_NORMAL   { 55,  55,  75  };
static constexpr sf::Color BTN_FILL_DISABLED { 35,  35,  45  };
static constexpr sf::Color BTN_FILL_ACTIVE   { 80,  80,  120 };
static constexpr sf::Color BTN_OUTLINE_NORMAL{ 100, 100, 140 };
static constexpr sf::Color BTN_OUTLINE_ACTIVE{ 140, 140, 220 };
static constexpr sf::Color BTN_TEXT_NORMAL   { 220, 220, 220 };
static constexpr sf::Color BTN_TEXT_DISABLED { 100, 100, 100 };

// --- Constantes de estilo del campo de texto ---
static constexpr unsigned int INPUT_FONT_SIZE       = 16;
static constexpr float        INPUT_TEXT_PADDING_X  = 10.f;
static constexpr float        INPUT_TEXT_PADDING_Y  = 10.f;

static constexpr sf::Color INPUT_FILL          { 25,  25,  35  };
static constexpr sf::Color INPUT_OUTLINE_NORMAL{ 80,  80,  110 };
static constexpr sf::Color INPUT_OUTLINE_FOCUS { 130, 130, 210 };
static constexpr sf::Color INPUT_PLACEHOLDER_COLOR{ 100, 100, 120 };

// Valores Unicode relevantes para la gestion de texto
static constexpr uint32_t UNICODE_BACKSPACE    = 8;  ///< Codigo Unicode de la tecla Retroceso
static constexpr uint32_t UNICODE_FIRST_PRINT  = 32; ///< Primer caracter imprimible ASCII
static constexpr uint32_t UNICODE_LAST_ASCII   = 127;///< Ultimo caracter ASCII valido

Button::Button(const sf::Font& font, const std::string& label,
               sf::Vector2f position, sf::Vector2f size)
    : m_text(font, label, BTN_FONT_SIZE)
    , m_position(position)
    , m_size(size)
{
    m_shape.setPosition(position);
    m_shape.setSize(size);
    m_shape.setFillColor(BTN_FILL_NORMAL);
    m_shape.setOutlineColor(BTN_OUTLINE_NORMAL);
    m_shape.setOutlineThickness(BTN_OUTLINE_W);

    m_text.setFillColor(BTN_TEXT_NORMAL);
    centerText();
}

void Button::draw(sf::RenderWindow& window) const
{
    window.draw(m_shape);
    window.draw(m_text);
}

bool Button::contains(sf::Vector2f point) const
{
    return m_enabled && m_shape.getGlobalBounds().contains(point);
}

void Button::setLabel(const std::string& label)
{
    m_text.setString(label);
    centerText();
}

void Button::setEnabled(bool enabled)
{
    m_enabled = enabled;
    m_shape.setFillColor(enabled ? BTN_FILL_NORMAL   : BTN_FILL_DISABLED);
    m_text.setFillColor (enabled ? BTN_TEXT_NORMAL   : BTN_TEXT_DISABLED);
}

void Button::setActive(bool active)
{
    m_shape.setFillColor   (active ? BTN_FILL_ACTIVE    : BTN_FILL_NORMAL);
    m_shape.setOutlineColor(active ? BTN_OUTLINE_ACTIVE : BTN_OUTLINE_NORMAL);
}

void Button::centerText()
{
    // En SFML 3, getLocalBounds() usa campos position y size.
    // Establecer el origen en el centro del bounding box centra el texto en el boton.
    const sf::FloatRect bounds = m_text.getLocalBounds();
    m_text.setOrigin({ bounds.position.x + bounds.size.x / 2.f,
                       bounds.position.y + bounds.size.y / 2.f });
    m_text.setPosition({ m_position.x + m_size.x / 2.f,
                         m_position.y + m_size.y / 2.f });
}

TextInput::TextInput(const sf::Font& font, sf::Vector2f position, sf::Vector2f size,
                     const std::string& placeholder, bool masked)
    : m_display(font, placeholder, INPUT_FONT_SIZE)
    , m_placeholder(placeholder)
    , m_masked(masked)
{
    m_shape.setPosition(position);
    m_shape.setSize(size);
    m_shape.setFillColor(INPUT_FILL);
    m_shape.setOutlineColor(INPUT_OUTLINE_NORMAL);
    m_shape.setOutlineThickness(BTN_OUTLINE_W);

    m_display.setPosition({ position.x + INPUT_TEXT_PADDING_X,
                            position.y + INPUT_TEXT_PADDING_Y });
    m_display.setFillColor(INPUT_PLACEHOLDER_COLOR);
}

void TextInput::handleEvent(const sf::Event& event)
{
    if (!m_focused) return;

    // TextEntered procesa caracteres con mayusculas, acentos y especiales correctamente.
    if (const sf::Event::TextEntered* te = event.getIf<sf::Event::TextEntered>())
    {
        if (te->unicode == UNICODE_BACKSPACE)
        {
            if (!m_text.empty())
                m_text.pop_back();
        }
        else if (te->unicode >= UNICODE_FIRST_PRINT &&
                 te->unicode <  UNICODE_LAST_ASCII)
        {
            m_text += static_cast<char>(te->unicode);
        }
        updateDisplay();
    }
}

void TextInput::draw(sf::RenderWindow& window) const
{
    window.draw(m_shape);
    window.draw(m_display);
}

void TextInput::setFocused(bool focused)
{
    m_focused = focused;
    m_shape.setOutlineColor(focused ? INPUT_OUTLINE_FOCUS  : INPUT_OUTLINE_NORMAL);
    m_shape.setOutlineThickness(focused ? BTN_OUTLINE_W_FOCUSED : BTN_OUTLINE_W);
}

bool TextInput::contains(sf::Vector2f point) const
{
    return m_shape.getGlobalBounds().contains(point);
}

void TextInput::clear()
{
    m_text.clear();
    updateDisplay();
}

void TextInput::updateDisplay()
{
    if (m_text.empty())
    {
        m_display.setString(m_placeholder);
        m_display.setFillColor(INPUT_PLACEHOLDER_COLOR);
    }
    else
    {
        const std::string shown = m_masked ? std::string(m_text.size(), '*') : m_text;
        m_display.setString(shown);
        m_display.setFillColor(sf::Color::White);
    }
}
