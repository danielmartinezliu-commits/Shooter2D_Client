#pragma once
#include <SFML/Graphics.hpp>
#include <string>

class Button
{
public:
    // Construye un boton con texto centrado.
    Button(const sf::Font& font, const std::string& label, sf::Vector2f position, sf::Vector2f size);

    // Dibuja el boton en la ventana.
    void draw(sf::RenderWindow& window) const;

    // Comprueba si un punto esta dentro del area del boton.
    bool contains(sf::Vector2f point) const;

    // Cambia el texto visible del boton y lo vuelve a centrar.
    void setLabel(const std::string& label);

    // Habilita o deshabilita el boton (afecta a contains y al color).
    void setEnabled(bool enabled);

    // Marca el boton como activo (usado para el tab seleccionado).
    void setActive(bool active);

private:
    // Centra el texto dentro del rectangulo del boton.
    void centerText();

    sf::RectangleShape m_shape; // Fondo del boton
    sf::Text m_text; // Texto centrado del boton
    sf::Vector2f m_position; // Posicion (esquina superior-izquierda)
    sf::Vector2f m_size; // Tamano del boton
    bool m_enabled = true; // false = gris y no interactivo
};

class TextInput
{
public:
    // Construye un campo de texto.

    TextInput(const sf::Font& font, sf::Vector2f position, sf::Vector2f size, const std::string& placeholder = "", bool masked = false);

    // Procesa un evento SFML para capturar pulsaciones de teclado.
    void handleEvent(const sf::Event& event);

    // Dibuja el campo de texto en la ventana.
    void draw(sf::RenderWindow& window) const;

    // Establece el foco del campo (determina si captura teclado).
    void setFocused(bool focused);

    // Comprueba si un punto esta dentro del area del campo.
    bool contains(sf::Vector2f point) const;

    // Borra el contenido del campo de texto.
    void clear();

    // Devuelve el texto actual introducido por el usuario.
    const std::string& getText() const { return m_text; }

private:
    // Actualiza el texto mostrado segun el estado actual (vacio/enmascarado).
    void updateDisplay();

    sf::RectangleShape m_shape;// Fondo del campo
    sf::Text m_display; // Texto mostrado (puede ser placeholder o asteriscos)
    std::string m_text; // Texto real introducido por el usuario
    std::string m_placeholder; // Texto de ayuda cuando el campo esta vacio
    bool m_focused = false; // true si captura eventos de teclado
    bool m_masked = false; // true para mostrar asteriscos
};
