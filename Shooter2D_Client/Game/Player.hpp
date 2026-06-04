#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "Map.hpp"

// Representa una bala disparada por un jugador.
struct Bullet
{
    sf::Vector2f pos; // Posicion actual (px)
    sf::Vector2f vel; // Velocidad (px/s)
    float lifetime = 2.f; // Tiempo de vida restante (segundos)
    bool active = true; // false cuando impacta o expira
};


class Player
{
public:
    // Constantes de fisica y disparo
    static constexpr float WIDTH = 24.f; // Ancho del hitbox (px)
    static constexpr float HEIGHT = 28.f; // Alto del hitbox (px)
    static constexpr float SPEED = 180.f; // Velocidad horizontal (px/s)
    static constexpr float JUMP_SPEED = -520.f;// Velocidad inicial de salto (px/s)
    static constexpr float GRAVITY = 900.f; // Aceleracion gravitatoria (px/s^2)
    static constexpr float BULLET_SPEED = 600.f; // Velocidad de las balas (px/s)
    static constexpr float SHOOT_COOLDOWN = 0.3f; // Tiempo entre disparos (segundos)

    // Construye un jugador en la posicion de spawn indicada.
    Player(sf::Vector2f spawnPos, sf::Color color);

    // Lee el estado del teclado y actualiza la velocidad y disparo del jugador local.
    void processInput();

    // Detiene el movimiento horizontal (usado cuando la ventana pierde el foco).
    void stopHorizontal();

    // Teletransporta al jugador a la posicion de resurreccion dada.
    void respawn(sf::Vector2f pos);

    // Actualiza la fisica y las balas del jugador un frame.
    void update(float dt, const Map& map, bool applyPhysics = true);

    // Actualiza la posicion del jugador remoto con datos recibidos del servidor.

    void setRemoteState(sf::Vector2f pos, bool facingRight);

    // Dibuja el jugador y sus balas en la ventana.
    void draw(sf::RenderWindow& window) const;

    sf::Vector2f position() const { return m_pos; }
    sf::Vector2f velocity() const { return m_vel; }
    bool isFacingRight() const { return m_facingRight; }
    const std::vector<Bullet>& bullets() const { return m_bullets; }
    std::vector<Bullet>& bullets() { return m_bullets; }

    // Indica si el jugador acaba de disparar en este frame.
    bool justFired() const { return m_justFired; }

    // Devuelve la posicion de la ultima bala disparada.
    sf::Vector2f lastBulletPos() const { return m_lastBulletPos; }

private:
    // Resuelve colisiones con tiles en el eje horizontal.
    void resolveX(const Map& map);

    // Resuelve colisiones con tiles en el eje vertical.
    void resolveY(const Map& map);

    // Actualiza la posicion de todas las balas activas y elimina las que expiran.
    void updateBullets(float dt, const Map& map);

    // Crea una nueva bala y activa la flag m_justFired.
    void fire();

    // Constantes de renderizado
    static constexpr float WEAPON_SIZE= 8.f;   // Tamano del cuadrado que representa el arma (px)
    static constexpr float BULLET_SIZE= 5.f;   // Tamano del cuadrado de la bala (px)
    static constexpr float WEAPON_OFFSET_R = 2.f;   // Separacion arma-jugador mirando derecha (px)
    static constexpr float WEAPON_OFFSET_L = 10.f;  // Separacion arma-jugador mirando izquierda (px)
    static constexpr float WEAPON_Y_RATIO = 0.3f;  // Posicion vertical del arma como fraccion de HEIGHT
    static constexpr int WEAPON_COLOR_DELTA = 80;   // Incremento de brillo del color del arma

    // Constantes de interpolacion del jugador remoto
    static constexpr float LERP_SPEED = 15.f; // Velocidad de interpolacion (s^-1); mayor = mas rapido
    static constexpr float SNAP_DISTANCE = 200.f; // Distancia (px) a partir de la cual se teletransporta

    // Estado del jugador
    sf::Vector2f  m_pos; // Posicion actual (esquina sup-izq del hitbox)
    sf::Vector2f m_targetPos; // Posicion objetivo recibida del servidor (solo jugador remoto)
    sf::Vector2f m_vel; // Velocidad actual (px/s)
    bool m_grounded = false; // true si el jugador esta en el suelo
    bool m_facingRight = true; // Direccion actual del jugador
    float m_shootTimer = 0.f; // Tiempo restante hasta el siguiente disparo
    bool m_justFired = false; // true durante un frame al disparar
    sf::Vector2f m_lastBulletPos;// Posicion de la ultima bala creada
    sf::Color m_color; // Color del jugador
    std::vector<Bullet> m_bullets; // Balas activas del jugador
};
