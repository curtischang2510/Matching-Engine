#include "Exchange.h"
#include "Networking/OrderProtoAdapter.h"
#include "Networking/ProtobufFrameReader.h"
#include "Networking/TcpServer.h"
#include "generated/order.pb.h"

#include <iostream>
#include <optional>
#include <unistd.h>

int main()
{
    TcpServer server{"localhost", 8001};

    int clientSocket = server.acceptClient();

    Exchange exchange;
    ProtobufFrameReader reader{clientSocket};

    while (true)
    {
        std::optional<std::string> payload = reader.readFrame();
        if (!payload)
        {
            break;
        }

        orderbook::Order order;
        if (!order.ParseFromString(*payload))
        {
            std::cerr << "Failed to parse protobuf order\n";
            continue;
        }

        std::optional<Trade> trade = toTrade(order);
        if (!trade)
        {
            std::cerr << "Invalid order received\n";
            continue;
        }

        exchange.addOrder(*trade);
    }

    ::close(clientSocket);
    return 0;
}
