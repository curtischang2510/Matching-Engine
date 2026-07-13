#ifndef MARKETMAKER_MULTICASTRECEIVER_H
#define MARKETMAKER_MULTICASTRECEIVER_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "Networking/SocketHandle.h"

class MulticastReceiver
{
public:
    MulticastReceiver(const char* group, std::uint16_t port, const char* iface)
    {
        int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0)
        {
            throw std::runtime_error("socket() failed");
        }
        sock_ = SocketHandle{fd};

        int yes = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        {
            throw std::runtime_error("bind() failed");
        }

        ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = ::inet_addr(group);
        mreq.imr_interface.s_addr = ::inet_addr(iface);
        if (::setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
        {
            throw std::runtime_error("IP_ADD_MEMBERSHIP failed");
        }

        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100 ms
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    // returns datagram size, or 0 on receive timeout
    std::size_t receive(std::uint8_t* buf, std::size_t cap)
    {
        const ssize_t n = ::recvfrom(sock_.fd(), buf, cap, 0, nullptr, nullptr);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return 0;
            }
            throw std::runtime_error("recvfrom() failed");
        }
        return static_cast<std::size_t>(n);
    }

    void sendRequest(const std::uint8_t* data, std::size_t size, const char* host, std::uint16_t port)
    {
        sockaddr_in dst{};
        dst.sin_family = AF_INET;
        dst.sin_port = htons(port);
        dst.sin_addr.s_addr = ::inet_addr(host);
        ::sendto(sock_.fd(), data, size, 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
    }

private:
    SocketHandle sock_;
};

#endif //MARKETMAKER_MULTICASTRECEIVER_H
