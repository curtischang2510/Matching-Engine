#include "ProtobufFrameReader.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>

namespace {

std::uint32_t parseBigEndianLength(const std::array<char, 4>& bytes)
{
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[0])) << 24) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[1])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[2])) << 8) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[3]));
}

} // namespace

ProtobufFrameReader::ProtobufFrameReader(int socketFd)
    : socketFd_(socketFd)
{
}

std::optional<std::string> ProtobufFrameReader::readFrame()
{
    std::array<char, 4> lengthBytes{};
    if (!readExactly(lengthBytes.data(), lengthBytes.size()))
    {
        return std::nullopt;
    }

    const std::uint32_t payloadSize = parseBigEndianLength(lengthBytes);
    if (payloadSize == 0)
    {
        return std::string{};
    }

    std::string payload(payloadSize, '\0');
    if (!readExactly(payload.data(), payload.size()))
    {
        return std::nullopt;
    }

    return payload;
}

bool ProtobufFrameReader::readExactly(char* buffer, std::size_t size)
{
    std::size_t bytesRead = 0;
    while (bytesRead < size)
    {
        const ssize_t result = ::recv(socketFd_, buffer + bytesRead, size - bytesRead, 0);
        if (result == 0)
        {
            return false;
        }

        if (result < 0)
        {
            throw std::runtime_error("recv failed: " + std::string(std::strerror(errno)));
        }

        bytesRead += static_cast<std::size_t>(result);
    }

    return true;
}
