# Simple TCP Server And Text Parser

## Position

The simple socket code is a good learning step. It is not automatically less
efficient than the RAII version. The main difference is safety and structure:

- Simple version: easier to read at first, but easy to forget `close`.
- RAII version: more code, but sockets close automatically when objects leave
  scope.

For your current goal, start simple. Once it works, wrap the socket fd in RAII.

## Minimal TCP Server

This version accepts one connection, reads one message, prints it, sends a small
response, and exits.

```cpp
#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    int serverSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        std::cerr << "socket failed: " << std::strerror(errno) << "\n";
        return 1;
    }

    int reuse = 1;
    ::setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8001);
    serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0)
    {
        std::cerr << "bind failed: " << std::strerror(errno) << "\n";
        ::close(serverSocket);
        return 1;
    }

    if (::listen(serverSocket, 1) < 0)
    {
        std::cerr << "listen failed: " << std::strerror(errno) << "\n";
        ::close(serverSocket);
        return 1;
    }

    std::cout << "Listening on localhost:8001\n";

    int clientSocket = ::accept(serverSocket, nullptr, nullptr);
    if (clientSocket < 0)
    {
        std::cerr << "accept failed: " << std::strerror(errno) << "\n";
        ::close(serverSocket);
        return 1;
    }

    std::array<char, 1024> buffer{};
    ssize_t bytesRead = ::recv(clientSocket, buffer.data(), buffer.size() - 1, 0);
    if (bytesRead < 0)
    {
        std::cerr << "recv failed: " << std::strerror(errno) << "\n";
    }
    else
    {
        buffer[static_cast<std::size_t>(bytesRead)] = '\0';
        std::cout << "Message from client: " << buffer.data() << "\n";

        const char response[] = "CONNECTED\n";
        ::send(clientSocket, response, sizeof(response) - 1, 0);
    }

    ::close(clientSocket);
    ::close(serverSocket);

    return 0;
}
```

## Why This Is Simpler Than The RAII Version

This code keeps everything in `main`.

That is useful while learning:

- You see `socket`.
- You see `bind`.
- You see `listen`.
- You see `accept`.
- You see `recv`.
- You see `send`.
- You see `close`.

The downside is that every error path must remember to close the right sockets.
That is where RAII becomes valuable later.

## Small RAII Upgrade

After the simple server works, the first cleanup is not a full `TcpServer` class.
The first cleanup is just a tiny fd owner:

```cpp
class SocketFd {
public:
    explicit SocketFd(int fd) : fd_(fd) {}

    ~SocketFd()
    {
        if (fd_ >= 0)
        {
            ::close(fd_);
        }
    }

    SocketFd(const SocketFd&) = delete;
    SocketFd& operator=(const SocketFd&) = delete;

    int get() const
    {
        return fd_;
    }

private:
    int fd_;
};
```

Then the destructor handles cleanup. That is the core RAII lesson.

## Important Networking Detail

The simple server above reads only once:

```cpp
recv(clientSocket, buffer.data(), buffer.size() - 1, 0);
```

That is enough to prove connectivity. It is not enough for a real message loop
because TCP is a byte stream. One `recv` call may contain:

- Half a message.
- Exactly one message.
- Multiple messages.

For the first connectivity test, this is fine. For processing orders, use a
line reader.

## Simple TCP Server Class

This version keeps the direct socket style, but moves server setup into a small
class. It is intentionally simpler than the earlier `getaddrinfo` version.

### `TcpServer.h`

```cpp
#ifndef ORDERBOOK_NETWORKING_TCPSERVER_H
#define ORDERBOOK_NETWORKING_TCPSERVER_H

#include <string>

class TcpServer {
public:
    TcpServer(int port);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    int acceptClient() const;

private:
    int serverSocket_;
};

#endif
```

### `TcpServer.cpp`

```cpp
#include "TcpServer.h"

#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

TcpServer::TcpServer(int port)
{
    serverSocket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0)
    {
        throw std::runtime_error("socket failed: " + std::string(std::strerror(errno)));
    }

    int reuse = 1;
    ::setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(serverSocket_, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0)
    {
        ::close(serverSocket_);
        throw std::runtime_error("bind failed: " + std::string(std::strerror(errno)));
    }

    if (::listen(serverSocket_, 1) < 0)
    {
        ::close(serverSocket_);
        throw std::runtime_error("listen failed: " + std::string(std::strerror(errno)));
    }
}

TcpServer::~TcpServer()
{
    if (serverSocket_ >= 0)
    {
        ::close(serverSocket_);
    }
}

int TcpServer::acceptClient() const
{
    int clientSocket = ::accept(serverSocket_, nullptr, nullptr);
    if (clientSocket < 0)
    {
        throw std::runtime_error("accept failed: " + std::string(std::strerror(errno)));
    }

    return clientSocket;
}
```

This is a small RAII improvement: the server socket closes in the destructor.
The client socket is still returned as a raw `int`, so the caller must close it.
That is fine for the first draft.

### Example `main.cpp`

```cpp
#include "Networking/TcpServer.h"

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    TcpServer server{8001};

    std::cout << "Listening on localhost:8001\n";

    int clientSocket = server.acceptClient();
    std::cout << "Client connected\n";

    const char response[] = "CONNECTED\n";
    ::send(clientSocket, response, sizeof(response) - 1, 0);

    ::close(clientSocket);
    return 0;
}
```

This proves the Python side can connect and receive a response.

## Line Reader For TCP

TCP is a byte stream, so you need a buffer that can handle partial messages and
multiple messages in one `recv`.

The feeder sends newline-delimited messages:

```text
AAPL,BID,100.50,10\n
MSFT,ASK,420.00,2\n
```

The server should process one complete line at a time.

### `LineReader.h`

```cpp
#ifndef ORDERBOOK_NETWORKING_LINEREADER_H
#define ORDERBOOK_NETWORKING_LINEREADER_H

#include <optional>
#include <string>

class LineReader {
public:
    explicit LineReader(int socketFd);

    std::optional<std::string> readLine();

private:
    int socketFd_;
    std::string buffer_;
};

#endif
```

### `LineReader.cpp`

```cpp
#include "LineReader.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>

LineReader::LineReader(int socketFd)
    : socketFd_(socketFd)
{
}

std::optional<std::string> LineReader::readLine()
{
    while (true)
    {
        std::size_t newline = buffer_.find('\n');
        if (newline != std::string::npos)
        {
            std::string line = buffer_.substr(0, newline);
            buffer_.erase(0, newline + 1);

            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            return line;
        }

        std::array<char, 4096> chunk{};
        ssize_t bytesRead = ::recv(socketFd_, chunk.data(), chunk.size(), 0);

        if (bytesRead == 0)
        {
            if (buffer_.empty())
            {
                return std::nullopt;
            }

            std::string finalLine = buffer_;
            buffer_.clear();
            return finalLine;
        }

        if (bytesRead < 0)
        {
            throw std::runtime_error("recv failed: " + std::string(std::strerror(errno)));
        }

        buffer_.append(chunk.data(), static_cast<std::size_t>(bytesRead));
    }
}
```

Return meaning:

- `std::string`: one complete line is ready.
- `std::nullopt`: client disconnected and no buffered data remains.
- exception: socket read failed.

### Example `main.cpp` With `LineReader`

```cpp
#include "Networking/LineReader.h"
#include "Networking/TcpServer.h"

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    TcpServer server{8001};

    std::cout << "Listening on localhost:8001\n";

    int clientSocket = server.acceptClient();
    std::cout << "Client connected\n";

    const char response[] = "CONNECTED\n";
    ::send(clientSocket, response, sizeof(response) - 1, 0);

    LineReader reader{clientSocket};

    while (true)
    {
        std::optional<std::string> line = reader.readLine();
        if (!line)
        {
            break;
        }

        std::cout << "Received line: " << *line << "\n";
    }

    ::close(clientSocket);
    return 0;
}
```

This is now safe against partial TCP reads. It can also handle multiple messages
arriving in one socket read.

## Efficient Text Parser

Your message format is:

```text
AAPL,BID,100.50,10
```

Fields:

```text
ticker,side,price,quantity
```

For a clean and reasonably efficient parser, use `std::string_view` to avoid
copying substrings while parsing.

### Parser Result

Use `std::optional<Trade>` for now:

```cpp
#include "Trade.h"

#include <charconv>
#include <optional>
#include <string>
#include <string_view>

std::optional<Trade> parseTrade(std::string_view line);
```

### Helper: Split One Field

```cpp
std::string_view takeField(std::string_view& input)
{
    const std::size_t comma = input.find(',');
    if (comma == std::string_view::npos)
    {
        std::string_view field = input;
        input = {};
        return field;
    }

    std::string_view field = input.substr(0, comma);
    input.remove_prefix(comma + 1);
    return field;
}
```

This mutates the `string_view`, not the original string.

### Helper: Parse Side

```cpp
std::optional<OrderType> parseSide(std::string_view side)
{
    if (side == "BID")
    {
        return OrderType::BID;
    }

    if (side == "ASK")
    {
        return OrderType::ASK;
    }

    return std::nullopt;
}
```

### Helper: Parse Integer Quantity

```cpp
std::optional<int64_t> parseQuantity(std::string_view text)
{
    int64_t value = 0;
    const char* begin = text.data();
    const char* end = text.data() + text.size();

    auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end || value <= 0)
    {
        return std::nullopt;
    }

    return value;
}
```

`std::from_chars` is good because it does not allocate and does not throw.

### Helper: Parse Price

For now, because your `Trade` still uses `double`, use `std::stod` on a temporary
string:

```cpp
std::optional<double> parsePrice(std::string_view text)
{
    try
    {
        std::size_t parsed = 0;
        double value = std::stod(std::string(text), &parsed);
        if (parsed != text.size() || value <= 0.0)
        {
            return std::nullopt;
        }

        return value;
    }
    catch (...)
    {
        return std::nullopt;
    }
}
```

This allocates because it creates a `std::string`. That is acceptable for the
first version.

Later, change price to integer ticks and parse with `std::from_chars`. That will
be cleaner and faster.

### Full Parser

```cpp
std::optional<Trade> parseTrade(std::string_view line)
{
    std::string_view input = line;

    std::string_view ticker = takeField(input);
    std::string_view sideText = takeField(input);
    std::string_view priceText = takeField(input);
    std::string_view qtyText = takeField(input);

    if (ticker.empty() || sideText.empty() || priceText.empty() || qtyText.empty())
    {
        return std::nullopt;
    }

    if (!input.empty())
    {
        return std::nullopt;
    }

    std::optional<OrderType> side = parseSide(sideText);
    std::optional<double> price = parsePrice(priceText);
    std::optional<int64_t> qty = parseQuantity(qtyText);

    if (!side || !price || !qty)
    {
        return std::nullopt;
    }

    return Trade{
        std::string(ticker),
        *side,
        *price,
        *qty
    };
}
```

## Why This Parser Is A Good First Version

Good:

- Avoids copying fields while splitting.
- Uses `std::from_chars` for quantity.
- Validates every field.
- Returns `std::optional` instead of crashing.
- Keeps parsing separate from sockets and `OrderBook`.

Not perfect yet:

- Price parsing allocates because of `std::stod(std::string(...))`.
- `double` is not ideal for prices.
- It assumes one complete line has already been received.

The next upgrade is to represent price as integer ticks:

```text
100.50 -> 10050
```

Then price parsing can use integer logic and avoid floating point.

## Build Order

1. Paste the minimal TCP server into `main.cpp`.
2. Run the C++ server.
3. Run the Python feeder.
4. Confirm the C++ server prints the received message.
5. Confirm Python can print the server response.
6. Move the parser into its own `TradeParser.h/.cpp`.
7. Unit test the parser without sockets.
8. Only then wire parser output into `Exchange::addOrder`.
