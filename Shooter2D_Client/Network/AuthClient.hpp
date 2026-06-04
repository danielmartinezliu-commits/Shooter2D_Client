#pragma once
#include <SFML/Network.hpp>
#include <future>
#include <string>
#include "Protocol.hpp"


struct AuthResult
{
    bool success = false; // true si la operacion fue exitosa
    std::string message; // Mensaje descriptivo del servidor
    int ranking = 0; // Ranking del usuario (valido si success=true)
};

class AuthClient
{
public:
    // Inicia un intento de login de forma asincrona.
    std::future<AuthResult> login(const std::string& host, unsigned short port, const std::string& username, const std::string& password);

    // Inicia un registro de nuevo usuario de forma asincrona.
    std::future<AuthResult> registerUser(const std::string& host, unsigned short port, const std::string& username, const std::string& password);

private:
    // Abre una conexion TCP, envia el paquete y espera la respuesta del servidor. Se ejecuta en el thread de std::async.
    AuthResult sendAndReceive(const std::string& host, unsigned short port, sf::Packet& packet);
};
