# This is a sample Python script.
from pathlib import Path


from encoder import ProtobufOrderEncoder
from market_data_feeder import MarketDataFeeder
from message_source import FileMessageSource
from transport import TcpTransport

DEFAULT_PATH = Path("fifty_thousand_data.txt")
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8001


def main():
    source = FileMessageSource(DEFAULT_PATH)
    encoder = ProtobufOrderEncoder()
    transport = TcpTransport(DEFAULT_HOST, DEFAULT_PORT)
    feeder = MarketDataFeeder(source, encoder, transport)
    feeder.run()

# Press the green button in the gutter to run the script.
if __name__ == '__main__':
    main()

# See PyCharm help at https://www.jetbrains.com/help/pycharm/
