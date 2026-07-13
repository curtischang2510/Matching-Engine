#ifndef ORDERBOOK_NETWORKING_SOCKETHANDLE_H
#define ORDERBOOK_NETWORKING_SOCKETHANDLE_H

#include <cstddef>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

class SocketHandle {
public:
    SocketHandle() = default;

    explicit SocketHandle(int fd)
        : fd_(fd)
    {
    }

    ~SocketHandle()
    {
        close();
    }

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;

    SocketHandle(SocketHandle&& other) noexcept
        : fd_(other.fd_)
    {
        other.fd_ = kInvalidFd;
    }

    SocketHandle& operator=(SocketHandle&& other) noexcept
    {
        if (this != &other)
        {
            close();
            fd_ = other.fd_;
            other.fd_ = kInvalidFd;
        }

        return *this;
    }

    int fd() const
    {
        return fd_;
    }

    bool isValid() const
    {
        return fd_ != kInvalidFd;
    }

    void close()
    {
        if (isValid())
        {
            ::close(fd_);
            fd_ = kInvalidFd;
        }
    }

    void sendAll(const std::string& message) const
    {
        sendAll(message.data(), message.size());
    }

    void sendAll(const char* data, std::size_t size) const
    {
        if (!isValid())
        {
            throw std::runtime_error("cannot send on invalid socket");
        }

        std::size_t bytesSent = 0;
        while (bytesSent < size)
        {
            const ssize_t result = ::send(fd_, data + bytesSent, size - bytesSent, 0);
            if (result < 0)
            {
                throw std::runtime_error("send failed");
            }

            bytesSent += static_cast<std::size_t>(result);
        }
    }

private:
    static constexpr int kInvalidFd = -1;

    int fd_ = kInvalidFd;
};

#endif
