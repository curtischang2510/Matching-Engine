#include "TcpServer.h"

TcpServer::TcpServer(const std::string& host, int port)
{
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        throw std::runtime_error("socket failed: " + std::string(std::strerror(errno)));
    }

    int reuse = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = (host == "0.0.0.0") ? htonl(INADDR_ANY) : htonl(INADDR_LOOPBACK);

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0)
    {
        close(serverSocket);
        throw std::runtime_error("bind failed: " + std::string(std::strerror(errno)));
    }

    if (listen(serverSocket, 1) < 0)
    {
        close(serverSocket);
        throw std::runtime_error("listen failed: " + std::string(std::strerror(errno)));
    }
}

TcpServer::~TcpServer()
{
    if (serverSocket >= 0)
    {
        close(serverSocket);
    }
}

int TcpServer::acceptClient() const
{
    int clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket < 0)
    {
        throw std::runtime_error("accept failed: " + std::string(std::strerror(errno)));

    }
    return clientSocket;
}

