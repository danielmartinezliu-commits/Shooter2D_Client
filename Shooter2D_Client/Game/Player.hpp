#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <deque>
#include <vector>
#include "Map.hpp"

// ================================================================
// Bullet — bala disparada por un jugador (cliente).
// ================================================================
struct Bullet
{
    sf::Vector2f pos;       // Posicion actual (px)
    sf::Vector2f vel;       // Velocidad (px/s)
    float lifetime = 2.f;  // Tiempo de vida restante (s)
    bool  active   = true; // false cuando impacta o expira
};

// ================================================================
// InputFlags — estado de las teclas de movimiento en un frame.
//
// SISTEMA 1 (Prediccion) + SISTEMA 3 (Reconciliacion):
// En vez de leer el teclado directamente dentro de la fisica,
// el input se codifica aqui para:
//   a) Enviarlo al servidor como PLAYER_INPUT.
//   b) Almacenarlo en el historial junto al estado resultante.
//   c) Re-aplicarlo exactamente durante la reconciliacion.
//
// El campo jump lleva la tecla cruda (sin filtro de grounded);
// el filtro se aplica dentro de moveStep/applyServerPhysics.
// ================================================================
struct InputFlags
{
    bool left  = false; // Izquierda / A
    bool right = false; // Derecha   / D
    bool jump  = false; // Arriba    / W (tecla cruda; moveStep filtra grounded)

    // Serializa a byte para enviar por PLAYER_INPUT (ver Protocol.hpp FLAG_*)
    uint8_t toUint8() const;

    // Deserializa desde un byte recibido
    static InputFlags fromUint8(uint8_t v);
};

// ================================================================
// InputRecord — entrada del historial de prediccion.
//
// SISTEMA 3 — RECONCILIACION:
// Se guarda una entrada por frame con el input enviado al servidor
// y el estado resultante de aplicarlo localmente. Cuando llega
// PLAYER_STATE, buscamos el registro con ese seq y comparamos.
// Si hay error mayor que el umbral, corregimos el estado y
// re-aplicamos todos los inputs posteriores (re-simulacion).
// ================================================================
struct InputRecord
{
    uint16_t   seq;              // Numero de secuencia del frame
    InputFlags flags;            // Input de ese frame
    float      dt;               // Delta-time de ese frame
    // Estado del jugador DESPUES de aplicar este input
    // (necesario para detectar error de prediccion y para restaurar
    //  el estado de partida antes de re-simular):
    sf::Vector2f posAfter;       // Posicion resultante
    float        vyAfter;        // Velocidad vertical resultante
    bool         groundedAfter;  // Estaba en el suelo al final del frame
    float        shootTimerAfter;// Cooldown de disparo al final del frame
    bool         facingRightAfter;// Direccion al final del frame
};

// ================================================================
// InterpSnapshot — muestra del estado del jugador remoto.
//
// SISTEMA 4 — INTERPOLACION:
// En vez de saltar directamente a la posicion recibida (o usar lerp),
// se guarda cada actualizacion con un timestamp de recepcion.
// Al renderizar se elige un instante 100ms en el pasado y se
// interpola linealmente entre los dos snapshots que lo rodean.
// Esto produce un movimiento suave aunque los paquetes lleguen
// con jitter o irregularidad.
// ================================================================
struct InterpSnapshot
{
    sf::Time     receivedAt; // Momento de recepcion (reloj interno del Player)
    sf::Vector2f pos;        // Posicion del jugador remoto en ese instante
    bool         facingRight;// Direccion en ese instante
};

// ================================================================
// AnimState — estado de la animacion del jugador.
// ================================================================
enum class AnimState { Idle, Run, Damage, Taunt };

// ================================================================
// Player — jugador local o remoto.
//
// JUGADOR LOCAL (prediccion + reconciliacion):
//   1. sampleInput()  — lee el teclado, sin efectos secundarios.
//   2. applyInput()   — aplica fisica, guarda historial, devuelve seq.
//   3. reconcile()    — corrige si el servidor discrepa.
//
// JUGADOR REMOTO (interpolacion):
//   1. addSnapshot()  — anade posicion recibida al buffer.
//   2. update()       — interpola posicion y avanza balas.
// ================================================================
class Player
{
public:
    // Constantes de fisica — DEBEN coincidir con las de GameSession
    // para que la prediccion del cliente sea precisa.
    static constexpr float WIDTH          = 24.f;
    static constexpr float HEIGHT         = 28.f;
    static constexpr float SPEED          = 180.f;
    static constexpr float JUMP_SPEED     = -520.f;
    static constexpr float GRAVITY        = 900.f;
    static constexpr float BULLET_SPEED   = 600.f;
    static constexpr float SHOOT_COOLDOWN =  0.3f;

    // Altura en pixels a la que se dibuja la Glock en pantalla.
    // Ajustar si la pistola queda demasiado grande o pequena.
    static constexpr float GLOCK_DISPLAY_H = 15.f;

    // @brief  Carga los tres assets (Player1.png, Player2.png, Glock.png)
    //         desde assetsPath. Llamar UNA sola vez antes de crear jugadores.
    //         Si falla, draw() usa el fallback de rectangulos de color.
    static void loadTextures(const std::string& assetsPath = "assets/");

    // @brief Construye un jugador en la posicion de spawn indicada.
    // @param playerIndex  0 = usa Player1.png, 1 = usa Player2.png.
    Player(sf::Vector2f spawnPos, sf::Color color, uint8_t playerIndex = 0);

    // ------------------------------------------------------------------
    // API jugador LOCAL
    // ------------------------------------------------------------------

    // @brief  Lee el teclado y devuelve flags de movimiento sin modificar
    //         el estado del jugador. Llamar antes de applyInput cada frame.
    InputFlags sampleInput() const;

    // @brief  SISTEMA 1 — PREDICCION.
    //         Aplica un paso de fisica con los flags dados de forma inmediata,
    //         sin esperar confirmacion del servidor. Registra el resultado
    //         en el historial para permitir la reconciliacion posterior.
    // @param  tryShoot  true solo cuando la ventana tiene foco y Space esta pulsado.
    //                   Se pasa como parametro en vez de leer el teclado aqui dentro
    //                   para evitar que sf::Keyboard::isKeyPressed (estado global del
    //                   SO) dispare en clientes sin foco cuando otro cliente pulsa Space.
    // @return Numero de secuencia del frame (enviar en PLAYER_INPUT).
    uint16_t applyInput(InputFlags flags, float dt, const Map& map, bool tryShoot = false);

    // @brief  SISTEMA 3 — RECONCILIACION.
    //         Compara la prediccion del cliente con el estado autoritativo
    //         del servidor. Si el error supera el umbral, corrige el estado
    //         actual y re-simula todos los inputs posteriores a ackSeq.
    // @param  ackSeq       Ultimo seq procesado por el servidor.
    // @param  serverPos    Posicion autoritativa.
    // @param  serverVy     Velocidad vertical autoritativa.
    // @param  serverGrounded Estado de contacto con el suelo.
    // @param  map          Para colision de tiles en la re-simulacion.
    void reconcile(uint16_t ackSeq,
                   sf::Vector2f serverPos,
                   float        serverVy,
                   bool         serverGrounded,
                   const Map&   map);

    // @brief  Detiene el movimiento horizontal (ventana sin foco).
    void stopHorizontal();

    // ------------------------------------------------------------------
    // API jugador REMOTO
    // ------------------------------------------------------------------

    // @brief  SISTEMA 4 — INTERPOLACION.
    //         Anade una nueva muestra al buffer de interpolacion.
    //         Llamar cada vez que llega PLAYER_POS del servidor.
    void addSnapshot(sf::Vector2f pos, bool facingRight);

    // @brief  SISTEMA 4 — INTERPOLACION.
    //         Interpola la posicion del jugador remoto desde el buffer
    //         (renderiza 100ms en el pasado) y avanza sus balas.
    //         Solo para uso en el jugador remoto.
    void update(float dt, const Map& map);

    // ------------------------------------------------------------------
    // API comun
    // ------------------------------------------------------------------

    // @brief  Activa la animacion de dano (frames 13-15).
    //         Llamar desde GameScreen al recibir PLAYER_HIT.
    void playDamageAnim();

    // @brief  Activa la animacion de burla (frames 16-23) y reproduce
    //         QuackSound.mp3. Llamar en el jugador local al pulsar la
    //         tecla de burla, y en el remoto al recibir PLAYER_TAUNT.
    //         No tiene efecto en el juego: es puramente cosmetica.
    void playTaunt();

    // @brief  true mientras la animacion de burla esta en curso.
    //         Util para evitar reenviar PLAYER_TAUNT en bucle mientras
    //         se mantiene pulsada la tecla.
    bool isTaunting() const { return m_animState == AnimState::Taunt; }

    // @brief  Avanza el estado de animacion un frame.
    //         Llamar una vez por frame para AMBOS jugadores (local y remoto).
    void tickAnimation(float dt);

    // @brief  Teletransporta al jugador a la posicion de resurreccion.
    void respawn(sf::Vector2f pos);

    // @brief  Dibuja el jugador y sus balas en la ventana.
    void draw(sf::RenderWindow& window) const;

    sf::Vector2f position()     const { return m_pos; }
    sf::Vector2f velocity()     const { return m_vel; }
    bool isFacingRight()        const { return m_facingRight; }
    bool justFired()            const { return m_justFired; }
    sf::Vector2f lastBulletPos()const { return m_lastBulletPos; }

    const std::vector<Bullet>& bullets() const { return m_bullets; }
    std::vector<Bullet>&       bullets()       { return m_bullets; }

private:
    // ------------------------------------------------------------------
    // Fisica interna
    // ------------------------------------------------------------------

    // @brief  Paso de fisica puro: mueve el jugador segun flags y dt,
    //         resuelve colisiones con tiles y actualiza m_shootTimer.
    //         Compartido por applyInput (prediccion) y reconcile
    //         (re-simulacion). NO dispara ni registra historial.
    void moveStep(InputFlags flags, float dt, const Map& map);

    void fire();
    void resolveX(const Map& map);
    void resolveY(const Map& map);
    void updateBullets(float dt, const Map& map);

    // ------------------------------------------------------------------
    // Renderizado
    // ------------------------------------------------------------------
    static constexpr float WEAPON_SIZE      =  8.f;
    static constexpr float BULLET_SIZE      =  5.f;
    static constexpr float WEAPON_OFFSET_R  =  0.5f;
    static constexpr float WEAPON_OFFSET_L  = 3.f;
    static constexpr float WEAPON_Y_RATIO   =  0.3f;
    static constexpr int   WEAPON_COLOR_DELTA = 80;

    // ------------------------------------------------------------------
    // Animacion
    // ------------------------------------------------------------------
    AnimState    m_animState   = AnimState::Idle;
    int          m_animFrame   = 0;
    float        m_animTimer   = 0.f;
    float        m_damageTimer = 0.f;   // Tiempo restante de la animacion de dano
    float        m_tauntTimer  = 0.f;   // Tiempo restante de la animacion de burla
    sf::Vector2f m_prevAnimPos;         // Para detectar movimiento del jugador remoto

    // ------------------------------------------------------------------
    // SISTEMA 3 — RECONCILIACION: parametros del historial
    // ------------------------------------------------------------------
    // Error maximo permitido (en px^2) antes de corregir.
    // Un umbral de 4 = ~2px de error; evita correcciones por
    // diferencias de punto flotante entre cliente y servidor.
    static constexpr float RECONCILE_THRESHOLD_SQ = 4.f;

    // Frames maximos en el historial. A 60fps y 200ms RTT se
    // necesitan ~12; 64 da margen para latencias altas.
    static constexpr int MAX_HISTORY = 64;

    // ------------------------------------------------------------------
    // SISTEMA 4 — INTERPOLACION: parametros del buffer
    // ------------------------------------------------------------------
    // El jugador remoto se renderiza INTERP_DELAY_S segundos en el
    // pasado para garantizar que siempre hay dos snapshots entre los
    // que interpolar, absorbiendo jitter de red.
    static constexpr float  INTERP_DELAY_S   = 0.10f; // 100ms de retardo de renderizado
    static constexpr size_t INTERP_BUFFER_MAX = 16;   // Maximos snapshots en buffer

    // ------------------------------------------------------------------
    // Estado del jugador
    // ------------------------------------------------------------------
    sf::Vector2f m_pos;
    sf::Vector2f m_vel;
    bool         m_grounded    = false;
    bool         m_facingRight = true;
    float        m_shootTimer  = 0.f;
    bool         m_justFired   = false;
    sf::Vector2f m_lastBulletPos;
    sf::Color    m_color;
    uint8_t      m_playerIndex = 0;     // 0 = Player1.png, 1 = Player2.png
    std::vector<Bullet> m_bullets;

    // SISTEMA 1 + 3: contador de secuencia e historial
    uint16_t m_inputSeq = 0;
    std::deque<InputRecord> m_history;

    // SISTEMA 4: buffer de interpolacion y reloj de timestamps
    std::deque<InterpSnapshot> m_snapshots;
    sf::Clock m_interpClock; // Lanzado en construccion; timestamps relativos
};
