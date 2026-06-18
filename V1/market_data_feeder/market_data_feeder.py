from transport import Transport
from encoder import NewlineTextEncoder
from message_source import FileMessageSource

class MarketDataFeeder:
    def __init__(self, source: FileMessageSource, encoder: NewlineTextEncoder, transport: Transport):
        self.source = source
        self.encoder = encoder
        self.transport = transport

    def run(self) -> None:
        with self.transport as transport:
            for message in self.source.messages():
                payload = self.encoder.encode(message)
                print(message)
                transport.send(payload)
