import struct

from generated import order_pb2


class NewlineTextEncoder:
    def encode(self, message: str) -> bytes:
        return (message + "\n").encode("ascii")


class ProtobufOrderEncoder:
    def encode(self, message: str) -> bytes:
        order = self._parse_order(message)
        payload = order.SerializeToString()
        return struct.pack("!I", len(payload)) + payload

    def _parse_order(self, message: str) -> order_pb2.Order:
        ticker, side_text, price_text, quantity_text = message.split(",")

        order = order_pb2.Order()
        order.stock_ticker = ticker
        order.side = self._parse_side(side_text)
        order.price = float(price_text)
        order.quantity = int(quantity_text)

        if order.quantity <= 0:
            raise ValueError("quantity must be positive")

        return order

    def _parse_side(self, side_text: str) -> int:
        if side_text == "BID":
            return order_pb2.BID

        if side_text == "ASK":
            return order_pb2.ASK

        raise ValueError(f"unknown side: {side_text}")
