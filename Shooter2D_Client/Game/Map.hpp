#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

class Map
{
public:
    static constexpr float TILE_SIZE = 32.f; // Tamano de cada tile en pixeles

    // Carga el mapa desde un fichero de texto ASCII.
    bool loadFromFile(const std::string& path);

    // Dibuja los tiles solidos del mapa en la ventana.
    void draw(sf::RenderWindow& window) const;

    // Comprueba si un tile es solido por coordenadas de tile.
    bool isSolid(int col, int row) const;

    // Comprueba si un tile es solido por posicion en coordenadas mundo.
    bool isSolid(sf::Vector2f worldPos) const;

    // Devuelve el punto de spawn de un jugador en coordenadas mundo.
    sf::Vector2f spawnPoint(int playerIndex) const;

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }

private:
    std::vector<std::string> m_grid; // Grid del mapa: m_grid[fila][columna]
    int m_cols = 0; // Numero de columnas
    int m_rows = 0; // Numero de filas
    sf::Vector2f m_spawns[2] = {}; // Puntos de spawn de cada jugador
};
