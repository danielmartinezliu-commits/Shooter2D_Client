#include "Player.hpp"
#include "../Network/Protocol.hpp"
#include <SFML/Audio.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>

// ================================================================
// Recursos estaticos compartidos por todos los jugadores
// ================================================================
static sf::Texture               s_texPlayer[2];
static sf::Texture               s_texGlock;
static sf::SoundBuffer           s_gunBuffer;
static std::optional<sf::Sound>  s_gunSound;   // optional: Sound requiere buffer en constructor
static bool                      s_texturesLoaded = false;
static int                       s_frameW         = 1;
static int                       s_frameH         = 1;
static int                       s_glockW         = 1;
static int                       s_glockH         = 1;

// Constantes de animacion (0-indexed sobre los 24 frames del spritesheet)
static constexpr int   ANIM_IDLE_START = 0;
static constexpr int   ANIM_IDLE_COUNT = 4;
static constexpr int   ANIM_RUN_START  = 4;
static constexpr int   ANIM_RUN_COUNT  = 9;
static constexpr int   ANIM_DMG_START  = 13;
static constexpr int   ANIM_DMG_COUNT  = 3;
static constexpr float ANIM_FPS        = 10.f; // Frames de animacion por segundo

void Player::loadTextures(const std::string& assetsPath)
{
    if (s_texturesLoaded) return;

    const bool ok0 = s_texPlayer[0].loadFromFile(assetsPath + "Player1.png");
    const bool ok1 = s_texPlayer[1].loadFromFile(assetsPath + "Player2.png");
    const bool okG = s_texGlock.loadFromFile(assetsPath + "Glock.png");

    if (!ok0 || !ok1 || !okG)
    {
        std::cerr << "[PLAYER] No se pudieron cargar los assets desde " << assetsPath
                  << " — usando fallback de rectangulos.\n";
        return;
    }

    // Calcular dimensiones de un frame: el spritesheet tiene 24 frames en fila
    s_frameW = static_cast<int>(s_texPlayer[0].getSize().x) / 24;
    s_frameH = static_cast<int>(s_texPlayer[0].getSize().y);
    s_glockW = static_cast<int>(s_texGlock.getSize().x);
    s_glockH = static_cast<int>(s_texGlock.getSize().y);

    // Cargar sonido de disparo (opcional: si falla, los disparos son silenciosos)
    if (s_gunBuffer.loadFromFile(assetsPath + "GunSound.mp3"))
    {
        s_gunSound.emplace(s_gunBuffer);
        s_gunSound->setVolume(60.f);
    }
    else
    {
        std::cerr << "[PLAYER] GunSound.mp3 no encontrado — disparos sin sonido.\n";
    }

    s_texturesLoaded = true;
    std::cout << "[PLAYER] Assets cargados | frame=" << s_frameW << "x" << s_frameH
              << " glock=" << s_glockW << "x" << s_glockH << "\n";
}

// ================================================================
// InputFlags
// ================================================================

uint8_t InputFlags::toUint8() const
{
    return (left  ? Protocol::FLAG_LEFT  : 0u)
         | (right ? Protocol::FLAG_RIGHT : 0u)
         | (jump  ? Protocol::FLAG_JUMP  : 0u);
}

InputFlags InputFlags::fromUint8(uint8_t v)
{
    return {
        (v & Protocol::FLAG_LEFT)  != 0,
        (v & Protocol::FLAG_RIGHT) != 0,
        (v & Protocol::FLAG_JUMP)  != 0
    };
}

// ================================================================
// Constructor / respawn / misc
// ================================================================

Player::Player(sf::Vector2f spawnPos, sf::Color color, uint8_t playerIndex)
    : m_pos(spawnPos.x + (Map::TILE_SIZE - WIDTH) * 0.5f, spawnPos.y)
    , m_prevAnimPos(m_pos)
    , m_color(color)
    , m_playerIndex(playerIndex % 2)
{
}

void Player::respawn(sf::Vector2f pos)
{
    m_pos       = pos;
    m_vel       = {};
    m_grounded  = false;
    m_justFired = false;
    m_bullets.clear();
    m_history.clear();   // Descartar historial: tras respawn el estado es autoritativo
    m_snapshots.clear(); // Descartar buffer de interpolacion
}

void Player::stopHorizontal()
{
    m_vel.x     = 0.f;
    m_justFired = false;
}

// ================================================================
// SISTEMA 1 — PREDICCION DEL CLIENTE
//
// sampleInput: lee el teclado sin modificar el estado del jugador.
// Separar la lectura del input de su aplicacion permite:
//   - Enviar los flags al servidor antes de aplicarlos.
//   - Re-aplicar inputs pasados durante la reconciliacion.
// ================================================================
InputFlags Player::sampleInput() const
{
    InputFlags f;
    f.left  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)
           || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A);
    f.right = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)
           || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D);
    // Jump: tecla cruda sin filtro de grounded. moveStep aplica el filtro.
    f.jump  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)
           || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W);
    return f;
}

// ================================================================
// SISTEMA 1 — PREDICCION DEL CLIENTE (continuacion)
//
// applyInput: aplica el input localmente de forma INMEDIATA, sin
// esperar confirmacion del servidor. Esto elimina el lag de input:
// el jugador responde al teclado en el mismo frame.
//
// Ademas guarda el estado resultante en m_history para que
// reconcile() pueda comparar con lo que diga el servidor.
// ================================================================
uint16_t Player::applyInput(InputFlags flags, float dt, const Map& map, bool tryShoot)
{
    const uint16_t seq = m_inputSeq++;
    m_justFired = false;

    // Aplicar paso de fisica (movimiento + colision)
    moveStep(flags, dt, map);

    // Disparo: no forma parte de la prediccion de movimiento ni de
    // la reconciliacion (las balas son autoritativas en el servidor).
    // Solo creamos la bala visual local y marcamos justFired para
    // que GameScreen envie PLAYER_SHOOT al servidor.
    if (tryShoot && m_shootTimer <= 0.f)
        fire();

    // Avanzar balas locales (visual; el servidor tiene sus propias)
    updateBullets(dt, map);

    // --- Guardar en historial ---
    // Almacenamos el estado POSTERIOR para que reconcile() pueda:
    //   1. Comparar posAfter con la posicion autoritativa del servidor.
    //   2. Restaurar shootTimer antes de re-simular (evita double-tick).
    InputRecord rec;
    rec.seq             = seq;
    rec.flags           = flags;
    rec.dt              = dt;
    rec.posAfter        = m_pos;
    rec.vyAfter         = m_vel.y;
    rec.groundedAfter   = m_grounded;
    rec.shootTimerAfter = m_shootTimer;
    rec.facingRightAfter= m_facingRight;
    m_history.push_back(rec);

    // Limitar tamano del historial para no acumular memoria infinitamente
    if (static_cast<int>(m_history.size()) > MAX_HISTORY)
        m_history.pop_front();

    return seq;
}

// ================================================================
// SISTEMA 3 — RECONCILIACION
//
// El servidor envia periodicamente PLAYER_STATE con su posicion
// autoritativa para el seq que acabo de procesar. Si nuestra
// prediccion difiere mas que RECONCILE_THRESHOLD_SQ, corregimos:
//
//   1. Localizar el InputRecord con ackSeq en el historial.
//   2. Purgar todas las entradas anteriores (ya confirmadas).
//   3. Si el error supera el umbral:
//      a. Aplicar el estado del servidor como punto de partida.
//      b. Re-simular todos los inputs posteriores a ackSeq.
//      c. Actualizar los posAfter del historial con los valores
//         corregidos (para que las proximas reconciliaciones sean precisas).
//
// En condiciones ideales (LAN, fisica determinista) el error es
// cero y esta funcion termina en el paso 2 sin corregir nada.
// ================================================================
void Player::reconcile(uint16_t ackSeq,
                       sf::Vector2f serverPos,
                       float        serverVy,
                       bool         serverGrounded,
                       const Map&   map)
{
    // --- Buscar ackSeq en el historial ---
    std::deque<InputRecord>::iterator it = std::find_if(m_history.begin(), m_history.end(),
                                                       [ackSeq](const InputRecord& r){ return r.seq == ackSeq; });

    if (it == m_history.end())
    {
        // El seq ya no esta en el historial (purgado o muy antiguo).
        // Correccion directa sin re-simulacion posible.
        m_pos      = serverPos;
        m_vel.y    = serverVy;
        m_grounded = serverGrounded;
        m_history.clear();
        return;
    }

    // --- Calcular error de prediccion ---
    const float errX  = serverPos.x - it->posAfter.x;
    const float errY  = serverPos.y - it->posAfter.y;
    const float errSq = errX * errX + errY * errY;

    // Restaurar shootTimer del momento ackSeq ANTES de borrar la entrada,
    // necesario para que la re-simulacion avance el cooldown correctamente.
    const float restoredShootTimer = it->shootTimerAfter;

    // --- Purgar entradas confirmadas (seq <= ackSeq) ---
    // Estas ya no sirven para nada: el servidor las proceso.
    m_history.erase(m_history.begin(), std::next(it));

    // Si el error es insignificante, la prediccion fue correcta: nada que hacer.
    if (errSq < RECONCILE_THRESHOLD_SQ)
        return;

    // ============================================================
    // CORRECCION + RE-SIMULACION
    // ============================================================
    // Partir del estado autoritativo del servidor en el instante ackSeq.
    m_pos        = serverPos;
    m_vel.y      = serverVy;
    m_grounded   = serverGrounded;
    m_shootTimer = restoredShootTimer;
    // m_vel.x y m_facingRight se derivan deterministamente de los flags
    // en cada paso de re-simulacion, no hace falta restaurarlos.

    // Re-aplicar todos los inputs que el servidor aun no ha confirmado.
    // Usamos moveStep (sin disparo ni historial) porque las balas ya se
    // crearon cuando el frame se proceso originalmente.
    for (InputRecord& rec : m_history)
    {
        moveStep(rec.flags, rec.dt, map);

        // Actualizar el historial con los estados corregidos para que
        // las proximas reconciliaciones puedan medir el error correctamente.
        rec.posAfter        = m_pos;
        rec.vyAfter         = m_vel.y;
        rec.groundedAfter   = m_grounded;
        rec.shootTimerAfter = m_shootTimer;
        rec.facingRightAfter= m_facingRight;
    }
    // Al terminar, m_pos es la mejor estimacion de la posicion actual:
    // estado autoritativo del servidor + re-simulacion de inputs no confirmados.
}

// ================================================================
// SISTEMA 4 — INTERPOLACION: jugador remoto
// ================================================================

void Player::addSnapshot(sf::Vector2f pos, bool facingRight)
{
    // Guardar con el timestamp actual del reloj interno
    m_snapshots.push_back({ m_interpClock.getElapsedTime(), pos, facingRight });

    // Mantener solo los ultimos INTERP_BUFFER_MAX snapshots
    if (m_snapshots.size() > INTERP_BUFFER_MAX)
        m_snapshots.pop_front();
}

// update() solo se usa para el jugador REMOTO.
// Renderiza la posicion que el jugador tenia hace INTERP_DELAY_S segundos
// interpolando entre los dos snapshots del buffer que rodean ese instante.
//
// Ventaja sobre lerp:
//   - El lerp simple siempre "persigue" la ultima posicion conocida,
//     produciendo movimiento que acelera o desacelera segun el lag.
//   - La interpolacion con buffer presenta movimiento uniforme a costa
//     de un pequeño retardo constante (100ms).
void Player::update(float dt, const Map& map)
{
    if (m_snapshots.size() < 2)
    {
        // Buffer insuficiente: snap directo al ultimo snapshot conocido.
        // Ocurre al inicio de la partida o tras una desconexion breve.
        if (!m_snapshots.empty())
        {
            m_pos        = m_snapshots.back().pos;
            m_facingRight= m_snapshots.back().facingRight;
        }
        updateBullets(dt, map);
        return;
    }

    // Tiempo objetivo: queremos mostrar al rival donde estaba hace INTERP_DELAY_S.
    // Renderizar en el pasado garantiza que siempre haya datos por delante.
    const sf::Time renderTime = m_interpClock.getElapsedTime() - sf::seconds(INTERP_DELAY_S);

    // Recorrer el buffer de mas reciente a mas antiguo buscando los dos
    // snapshots que rodean renderTime (older <= renderTime < newer).
    for (int i = static_cast<int>(m_snapshots.size()) - 1; i >= 1; --i)
    {
        const InterpSnapshot& newer = m_snapshots[i];
        const InterpSnapshot& older = m_snapshots[i - 1];

        if (older.receivedAt <= renderTime)
        {
            // Calcular el factor de interpolacion t en [0, 1]
            const float span = (newer.receivedAt - older.receivedAt).asSeconds();
            if (span < 0.001f)
            {
                m_pos = newer.pos; // Snapshots casi simultaneos: usar el mas reciente
            }
            else
            {
                const float t = std::min(
                    (renderTime - older.receivedAt).asSeconds() / span,
                    1.f // Clamped: no extrapolar mas alla de newer
                );
                m_pos = older.pos + (newer.pos - older.pos) * t;
            }
            m_facingRight = newer.facingRight; // La direccion no necesita interpolacion
            updateBullets(dt, map);
            return;
        }
    }

    // renderTime es anterior a todos los snapshots: mostrar el mas antiguo.
    // Ocurre los primeros instantes si INTERP_DELAY es mayor que el tiempo
    // transcurrido desde el primer snapshot.
    m_pos        = m_snapshots.front().pos;
    m_facingRight= m_snapshots.front().facingRight;
    updateBullets(dt, map);
}

// ================================================================
// Fisica interna
// ================================================================

// moveStep: unico metodo que modifica m_pos/m_vel.
// Tanto applyInput (prediccion) como reconcile (re-simulacion)
// pasan por aqui para garantizar que ambas usan exactamente
// el mismo codigo — condicion necesaria para que la prediccion sea precisa.
void Player::moveStep(InputFlags flags, float dt, const Map& map)
{
    // --- Velocidad horizontal ---
    if (flags.right)      { m_vel.x =  SPEED; m_facingRight = true;  }
    else if (flags.left)  { m_vel.x = -SPEED; m_facingRight = false; }
    else                    m_vel.x = 0.f;

    // --- Salto: solo si esta en el suelo ---
    if (flags.jump && m_grounded)
    {
        m_vel.y    = JUMP_SPEED;
        m_grounded = false;
    }

    // --- Avanzar cooldown de disparo ---
    m_shootTimer -= dt;

    // --- Gravedad ---
    m_vel.y += GRAVITY * dt;

    // --- Movimiento X + colision ---
    // X se resuelve antes que Y para evitar corner-sticking en esquinas.
    m_pos.x += m_vel.x * dt;
    resolveX(map);

    // --- Movimiento Y + colision ---
    m_grounded = false;
    m_pos.y += m_vel.y * dt;
    resolveY(map);
}

// ================================================================
// Animacion
// ================================================================

void Player::playDamageAnim()
{
    m_animState   = AnimState::Damage;
    m_animFrame   = ANIM_DMG_START;
    m_animTimer   = 0.f;
    m_damageTimer = static_cast<float>(ANIM_DMG_COUNT) / ANIM_FPS;
}

void Player::tickAnimation(float dt)
{
    // Determinar si el jugador esta en movimiento.
    // Para el jugador local m_vel.x refleja el input del frame.
    // Para el jugador remoto usamos el delta de posicion (m_vel.x no se actualiza).
    const float posDelta = std::abs(m_pos.x - m_prevAnimPos.x);
    const bool  moving   = (std::abs(m_vel.x) > 1.f) || (posDelta > 0.5f);
    m_prevAnimPos = m_pos;

    if (m_animState == AnimState::Damage)
    {
        m_damageTimer -= dt;
        if (m_damageTimer <= 0.f)
        {
            // Fin de la animacion de dano: volver a idle o run
            m_animState = moving ? AnimState::Run : AnimState::Idle;
            m_animFrame = moving ? ANIM_RUN_START : ANIM_IDLE_START;
            m_animTimer = 0.f;
        }
        else
        {
            m_animTimer += dt;
            if (m_animTimer >= 1.f / ANIM_FPS)
            {
                m_animTimer = 0.f;
                ++m_animFrame;
                // Repetir los frames de dano hasta que expire el timer
                if (m_animFrame >= ANIM_DMG_START + ANIM_DMG_COUNT)
                    m_animFrame = ANIM_DMG_START;
            }
        }
        return;
    }

    // Transicion idle <-> run
    const AnimState target = moving ? AnimState::Run : AnimState::Idle;
    if (m_animState != target)
    {
        m_animState = target;
        m_animFrame = moving ? ANIM_RUN_START : ANIM_IDLE_START;
        m_animTimer = 0.f;
    }

    // Avanzar frame
    m_animTimer += dt;
    if (m_animTimer >= 1.f / ANIM_FPS)
    {
        m_animTimer = 0.f;
        ++m_animFrame;
        if (m_animState == AnimState::Idle && m_animFrame >= ANIM_IDLE_START + ANIM_IDLE_COUNT)
            m_animFrame = ANIM_IDLE_START;
        else if (m_animState == AnimState::Run && m_animFrame >= ANIM_RUN_START + ANIM_RUN_COUNT)
            m_animFrame = ANIM_RUN_START;
    }
}

void Player::fire()
{
    const float bx = m_facingRight
        ? m_pos.x + WIDTH + WEAPON_OFFSET_R
        : m_pos.x - WEAPON_OFFSET_L;
    const float by = m_pos.y + HEIGHT * WEAPON_Y_RATIO;

    m_bullets.push_back({ {bx, by}, {m_facingRight ? BULLET_SPEED : -BULLET_SPEED, 0.f} });
    m_lastBulletPos = { bx, by };
    m_justFired     = true;
    m_shootTimer    = SHOOT_COOLDOWN;

    if (s_gunSound)
        s_gunSound->play();
}

void Player::resolveX(const Map& map)
{
    const float ts     = Map::TILE_SIZE;
    const int   topRow = static_cast<int>(m_pos.y / ts);
    const int   botRow = static_cast<int>((m_pos.y + HEIGHT - 1.f) / ts);

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
    const float ts       = Map::TILE_SIZE;
    const int   leftCol  = static_cast<int>(m_pos.x / ts);
    const int   rightCol = static_cast<int>((m_pos.x + WIDTH - 1.f) / ts);

    if (m_vel.y > 0.f)
    {
        const int row = static_cast<int>((m_pos.y + HEIGHT - 1.f) / ts);
        if (map.isSolid(leftCol, row) || map.isSolid(rightCol, row))
        {
            m_pos.y    = row * ts - HEIGHT;
            m_vel.y    = 0.f;
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
        b.pos     += b.vel * dt;
        b.lifetime -= dt;
        if (b.lifetime <= 0.f || map.isSolid(b.pos))
            b.active = false;
    }
    m_bullets.erase(
        std::remove_if(m_bullets.begin(), m_bullets.end(),
                       [](const Bullet& b){ return !b.active; }),
        m_bullets.end());
}

// ================================================================
// Renderizado
// ================================================================

void Player::draw(sf::RenderWindow& window) const
{
    if (!s_texturesLoaded)
    {
        // ---- Fallback: rectangulos de color (sin assets) ----
        sf::RectangleShape body({ WIDTH, HEIGHT });
        body.setFillColor(m_color);
        body.setPosition(m_pos);
        window.draw(body);

        const float wx = m_facingRight
            ? m_pos.x + WIDTH + WEAPON_OFFSET_R
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
    }
    else
    {
        // ---- Sprite del personaje ----
        // El spritesheet mira a la derecha. Cuando el jugador mira a la
        // izquierda se aplica escala negativa en X y se desplaza la posicion
        // para que el sprite siga alineado con la esquina superior-izquierda
        // del hitbox (m_pos).
        const float scaleX = WIDTH  / static_cast<float>(s_frameW);
        const float scaleY = HEIGHT / static_cast<float>(s_frameH);

        sf::IntRect frameRect({ m_animFrame * s_frameW, 0 }, { s_frameW, s_frameH });
        sf::Sprite sprite(s_texPlayer[m_playerIndex]);
        sprite.setTextureRect(frameRect);

        if (m_facingRight)
        {
            sprite.setScale({ scaleX, scaleY });
            sprite.setPosition(m_pos);
        }
        else
        {
            // Escala negativa en X: el pixel (0,0) queda en m_pos.x + WIDTH
            sprite.setScale({ -scaleX, scaleY });
            sprite.setPosition({ m_pos.x + WIDTH, m_pos.y });
        }
        window.draw(sprite);

        // ---- Glock ----
        // La Glock.png mira a la IZQUIERDA.
        //   Mirando izquierda → dibujar tal cual (sin flip).
        //   Mirando derecha   → flip horizontal (escala negativa en X).
        //
        // Con escala negativa, SFML expande el sprite hacia la izquierda
        // desde su posicion. Para que el borde derecho de la pistola
        // quede junto al jugador cuando mira derecha, compensamos sumando
        // glockDispW a la posicion X.
        const float glockScale  = GLOCK_DISPLAY_H / static_cast<float>(s_glockH);
        const float glockDispW  = static_cast<float>(s_glockW) * glockScale;
        const float wy          = m_pos.y + HEIGHT * WEAPON_Y_RATIO;

        sf::Sprite glock(s_texGlock);

        if (m_facingRight)
        {
            // Flip: la pistola queda a la derecha del personaje mirando derecha
            glock.setScale({ -glockScale, glockScale });
            glock.setPosition({ m_pos.x + WIDTH + WEAPON_OFFSET_R + glockDispW, wy });
        }
        else
        {
            // Sin flip: la pistola queda a la izquierda del personaje mirando izquierda
            glock.setScale({ glockScale, glockScale });
            glock.setPosition({ m_pos.x - WEAPON_OFFSET_L - glockDispW, wy });
        }
        window.draw(glock);
    }

    // ---- Balas (siempre, independiente de si hay assets) ----
    sf::RectangleShape bulletShape({ BULLET_SIZE, BULLET_SIZE });
    bulletShape.setFillColor(sf::Color::Yellow);
    for (const Bullet& b : m_bullets)
    {
        bulletShape.setPosition(b.pos);
        window.draw(bulletShape);
    }
}
