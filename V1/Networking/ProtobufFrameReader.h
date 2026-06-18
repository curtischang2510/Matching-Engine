#ifndef ORDERBOOK_NETWORKING_PROTOBUFFRAMEREADER_H
#define ORDERBOOK_NETWORKING_PROTOBUFFRAMEREADER_H

#include <cstdint>
#include <optional>
#include <string>

class ProtobufFrameReader {
public:
    explicit ProtobufFrameReader(int socketFd);

    std::optional<std::string> readFrame();

private:
    bool readExactly(char* buffer, std::size_t size);

    int socketFd_;
};

#endif
