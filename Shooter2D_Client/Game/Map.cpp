#include "Map.hpp"
#include <fstream>
#include <iostream>

bool Map::loadFromFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "[MAP] No se puede abrir: " << path << "\n";
        return false;
    }

    m_grid.clear();
    m_spawns[0] = m_spawns[1] = {};

    std::string line;
    int row = 0;
    while (std::getline(file, line))
    {
        for (int col = 0; col < static_cast<int>(line.size()); ++col)
        {
            const char c = line[col];
            if (c == '1' || c == '2')
            {
                m_spawns[c - '1'] = { col * TILE_SIZE, row * TILE_SIZE };
                line[col] = ' ';
            }
        }
        m_grid.push_back(std::move(line));
        ++row;
    }

    m_rows = static_cast<int>(m_grid.size());
    m_cols = m_rows > 0 ? static_cast<int>(m_grid[0].size()) : 0;

    std::cout << "[MAP] " << m_cols << "x" << m_rows << " cargado desde " << path << "\n";
    return true;
}

void Map::draw(sf::RenderWindow& window) const
{
    // 1px de separacion entre tiles para efecto de rejilla
    sf::RectangleShape tile({ TILE_SIZE - 1.f, TILE_SIZE - 1.f });
    tile.setFillColor(sf::Color(80, 85, 110));

    for (int r = 0; r < m_rows; ++r)
    {
        for (int c = 0; c < static_cast<int>(m_grid[r].size()); ++c)
        {
            if (m_grid[r][c] == '#')
            {
                tile.setPosition({ c * TILE_SIZE, r * TILE_SIZE });
                window.draw(tile);
            }
        }
    }
}

bool Map::isSolid(int col, int row) const
{
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols)
        return true;  // fuera de limites = solido
    return m_grid[row][col] == '#';
}

bool Map::isSolid(sf::Vector2f worldPos) const
{
    return isSolid(
        static_cast<int>(worldPos.x / TILE_SIZE),
        static_cast<int>(worldPos.y / TILE_SIZE)
    );
}

sf::Vector2f Map::spawnPoint(int playerIndex) const
{
    if (playerIndex < 0 || playerIndex > 1) return {};
    return m_spawns[playerIndex];
}
