#ifndef ORDERBOOK_NETWORKING_TCPSERVER_H
#define ORDERBOOK_NETWORKING_TCPSERVER_H

#include "SocketHandle.h"

#include <string>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

class TcpServer {
public:
    TcpServer(const std::string& host, int port);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    int acceptClient() const;

private:
    int serverSocket;
};

#endif
