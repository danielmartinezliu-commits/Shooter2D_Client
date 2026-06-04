#include "AuthClient.hpp"
#include <future>

// Timeout de conexion TCP al servidor de autenticacion (segundos)
static constexpr float CONNECTION_TIMEOUT_S = 5.f;

std::future<AuthResult> AuthClient::login(const std::string& host, unsigned short port,
                                           const std::string& username,
                                           const std::string& password)
{
    return std::async(std::launch::async, [=]() mutable -> AuthResult
    {
        sf::Packet packet;
        // Serializacion del paquete de login:
        //   [uint8_t: LOGIN_REQUEST][string: username][string: password]
        // sf::Packet antepone 4 bytes con el tamano total antes de enviar.
        packet << static_cast<uint8_t>(Protocol::MessageType::LOGIN_REQUEST)
               << username << password;
        return sendAndReceive(host, port, packet);
    });
}

std::future<AuthResult> AuthClient::registerUser(const std::string& host, unsigned short port,
                                                   const std::string& username,
                                                   const std::string& password)
{
    return std::async(std::launch::async, [=]() mutable -> AuthResult
    {
        sf::Packet packet;
        packet << static_cast<uint8_t>(Protocol::MessageType::REGISTER_REQUEST)
               << username << password;
        return sendAndReceive(host, port, packet);
    });
}

AuthResult AuthClient::sendAndReceive(const std::string& host, unsigned short port,
                                       sf::Packet& packet)
{
    // En SFML 3, sf::IpAddress no tiene constructor implicito desde std::string.
    // resolve() hace la resolucion DNS si es necesario y devuelve std::optional.
    const std::optional<sf::IpAddress> address = sf::IpAddress::resolve(host);
    if (!address)
        return { false, "Direccion de servidor invalida: " + host };

    sf::TcpSocket socket;

    // connect() ejecuta el three-way handshake TCP con el servidor.
    // El timeout de 5 segundos evita que la app se quede colgada si el servidor no responde.
    if (socket.connect(*address, port, sf::seconds(CONNECTION_TIMEOUT_S)) != sf::Socket::Status::Done)
        return { false, "No se pudo conectar al servidor" };

    if (socket.send(packet) != sf::Socket::Status::Done)
        return { false, "Error al enviar datos" };

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
        return { false, "Error al recibir respuesta" };

    // Deserializacion de la respuesta del servidor:
    // [uint8_t: AUTH_SUCCESS/AUTH_FAIL][string: mensaje][int32_t: ranking]
    uint8_t rawType{};
    std::string message;
    int ranking = 0;
    response >> rawType >> message >> ranking;

    const bool success = (static_cast<Protocol::MessageType>(rawType) == Protocol::MessageType::AUTH_SUCCESS);
    return { success, message, ranking };

    // El destructor de sf::TcpSocket cierra la conexion (FIN TCP)
}
