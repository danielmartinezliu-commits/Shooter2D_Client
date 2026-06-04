#pragma once
#include <string>

class Config
{
public:
    // Devuelve la instancia unica del singleton de configuracion.
    static Config& instance();

    // Carga los parametros de conexion desde un fichero .ini. Si el fichero no existe, mantiene los valores por defecto.
    void load(const std::string& filePath);

    std::string    serverIP = "127.0.0.1"; // IP del servidor (fallback: localhost)
    unsigned short authPort = 45000; // Puerto TCP de autenticacion y matchmaking
    unsigned short gamePort = 45001; // Puerto UDP del servidor de juego

private:
    Config() = default; // Constructor privado: usar instance()
};
