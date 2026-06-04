#include "Player.hpp"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <cmath>

Player::Player(sf::Vector2f spawnPos, sf::Color color)
    : m_pos(spawnPos.x + (Map::TILE_SIZE - WIDTH) * 0.5f, spawnPos.y)
    , m_targetPos(m_pos)
    , m_color(color)
{
}

void Player::respawn(sf::Vector2f pos)
{
    m_pos = pos;
    m_targetPos = pos;  // Snap inmediato: no interpolar en resurreccion
    m_vel = {};
    m_grounded  = false;
    m_justFired = false;
    m_bullets.clear();
}

void Player::stopHorizontal()
{
    m_vel.x = 0.f;
    m_justFired = false;
}

void Player::processInput()
{
    m_justFired = false;
    m_vel.x = 0.f;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
    {
        m_vel.x = -SPEED;
        m_facingRight = false;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
    {
        m_vel.x = SPEED;
        m_facingRight = true;
    }

    if ((sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up) ||
         sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) && m_grounded)
    {
        m_vel.y = JUMP_SPEED;
        m_grounded = false;
    }

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) && m_shootTimer <= 0.f)
        fire();
}

void Player::fire()
{
    // El arma aparece delante del jugador segun la direccion que mira
    const float bx = m_facingRight ? m_pos.x + WIDTH + WEAPON_OFFSET_R : m_pos.x - WEAPON_OFFSET_L;
    const float by = m_pos.y + HEIGHT * WEAPON_Y_RATIO;

    m_bullets.push_back({ {bx, by}, {m_facingRight ? BULLET_SPEED : -BULLET_SPEED, 0.f} });
    m_lastBulletPos = { bx, by };
    m_justFired = true;
    m_shootTimer = SHOOT_COOLDOWN;
}

void Player::update(float dt, const Map& map, bool applyPhysics)
{
    m_shootTimer -= dt;

    if (applyPhysics)
    {
        m_vel.y += GRAVITY * dt;

        // Resolver colision en X antes que en Y para evitar corner-sticking
        m_pos.x += m_vel.x * dt;
        resolveX(map);

        m_grounded = false;
        m_pos.y += m_vel.y * dt;
        resolveY(map);
    }
    else
    {
        // Interpolacion lineal hacia la posicion objetivo del jugador remoto.
        // Clampear el factor a 1.0 evita que se supere el objetivo (overshoot).
        const sf::Vector2f delta = m_targetPos - m_pos;
        const float distSq = delta.x * delta.x + delta.y * delta.y;

        if (distSq > SNAP_DISTANCE * SNAP_DISTANCE)
            m_pos = m_targetPos;  // Snap si la distancia es excesiva (lag o respawn)
        else
            m_pos += delta * std::min(LERP_SPEED * dt, 1.f);
    }

    updateBullets(dt, map);
}

void Player::setRemoteState(sf::Vector2f pos, bool facingRight)
{
    m_targetPos = pos; // Solo actualizamos el objetivo; m_pos se mueve por lerp en update()
    m_facingRight = facingRight;  // La direccion se aplica inmediatamente (no necesita interpolacion)
}

void Player::resolveX(const Map& map)
{
    const float ts = Map::TILE_SIZE;
    const int topRow = static_cast<int>(m_pos.y / ts);
    const int botRow = static_cast<int>((m_pos.y + HEIGHT - 1.f) / ts);

    if (m_vel.x > 0.f)
    {
        const int col = static_cast<int>((m_pos.x + WIDTH - 1.f) / ts);
        if (map.isSolid(col, topRow) || map.isSolid(col, botRow))
        {
            m_pos.x = col * ts - WIDTH;
            m_vel.x = 0.f;
        }
    }
    else if (m_vel.x < 0.f)
    {
        const int col = static_cast<int>(m_pos.x / ts);
        if (map.isSolid(col, topRow) || map.isSolid(col, botRow))
        {
            m_pos.x = (col + 1) * ts;
            m_vel.x = 0.f;
        }
    }
}

void Player::resolveY(const Map& map)
{
    const float ts = Map::TILE_SIZE;
    const int leftCol = static_cast<int>(m_pos.x / ts);
    const int rightCol = static_cast<int>((m_pos.x + WIDTH - 1.f) / ts);

    if (m_vel.y > 0.f)
    {
        const int row = static_cast<int>((m_pos.y + HEIGHT - 1.f) / ts);
        if (map.isSolid(leftCol, row) || map.isSolid(rightCol, row))
        {
            m_pos.y = row * ts - HEIGHT;
            m_vel.y = 0.f;
            m_grounded = true;
        }
    }
    else if (m_vel.y < 0.f)
    {
        const int row = static_cast<int>(m_pos.y / ts);
        if (map.isSolid(leftCol, row) || map.isSolid(rightCol, row))
        {
            m_pos.y = (row + 1) * ts;
            m_vel.y = 0.f;
        }
    }
}

void Player::updateBullets(float dt, const Map& map)
{
    for (Bullet& b : m_bullets)
    {
        if (!b.active) continue;
        b.pos += b.vel * dt;
        b.lifetime -= dt;
        if (b.lifetime <= 0.f || map.isSolid(b.pos))
            b.active = false;
    }

    m_bullets.erase(std::remove_if(m_bullets.begin(), m_bullets.end(), [](const Bullet& b) { return !b.active; }), m_bullets.end());
}

void Player::draw(sf::RenderWindow& window) const
{
    // Cuerpo del jugador
    sf::RectangleShape body({ WIDTH, HEIGHT });
    body.setFillColor(m_color);
    body.setPosition(m_pos);
    window.draw(body);

    // Arma: cuadrado delante segun la direccion; ligeramente mas claro que el cuerpo
    const float wx = m_facingRight ? m_pos.x + WIDTH + WEAPON_OFFSET_R
                                   : m_pos.x - WEAPON_OFFSET_L;
    const float wy = m_pos.y + HEIGHT * WEAPON_Y_RATIO;

    sf::RectangleShape weapon({ WEAPON_SIZE, WEAPON_SIZE });
    weapon.setFillColor(sf::Color(
        static_cast<uint8_t>(std::min(255, static_cast<int>(m_color.r) + WEAPON_COLOR_DELTA)),
        static_cast<uint8_t>(std::min(255, static_cast<int>(m_color.g) + WEAPON_COLOR_DELTA)),
        static_cast<uint8_t>(std::min(255, static_cast<int>(m_color.b) + WEAPON_COLOR_DELTA))
    ));
    weapon.setPosition({ wx, wy });
    window.draw(weapon);

    // Balas activas del jugador
    sf::RectangleShape bulletShape({ BULLET_SIZE, BULLET_SIZE });
    bulletShape.setFillColor(sf::Color::Yellow);
    for (const Bullet& b : m_bullets)
    {
        bulletShape.setPosition(b.pos);
        window.draw(bulletShape);
    }
}
