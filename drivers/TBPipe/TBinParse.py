import socket
import struct


class TBinClient:
    def __init__(self, address):
        """
           address = (host, port)
        """
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect(address)
        pass
    
    def calc_crc(self, packet_id : int, packet_size : int):
        return (0xFFFF ^ (packet_size&0xFFFF) ^ ( (packet_size>>16)&0xFFFF )  ^ packet_id) & 0xFFFF

    def comm(self, packet_id : int, packet_data : bytearray):
        """
          Отсылает пакет (packet_id, packet_data), принимает ответный пакет.
          Формат принимаемых и посылаемых сообщений таков.
          4 байта - заголовок, потом данные.
          Формат заголовка.
          uint32 data_size
          uint16 packet_id
          uint16 crc = 0xFFFF ^ (data_size&0xFFFF) ^ ((data_size>>16)&0xFFFF) ^ packet_id
        """
        out_size = len(packet_data)
        out_crc = self.calc_crc(packet_id, out_size)
        self.socket.sendall(struct.pack("IHH", out_size, packet_id, out_crc))
        self.socket.sendall(packet_data)

        in_header = self.socket.recv(8)
        (in_size, in_packet_id, in_crc) = struct.unpack("IHH", in_header)
        in_crc_calculated = self.calc_crc(in_packet_id, in_size)
        assert (in_crc_calculated==in_crc)
        in_data = self.socket.recv(in_size)
        return (in_packet_id, in_data)

if __name__ == "__main__":
    client = TBinClient(address=("127.0.0.1", 5002))
    (out_packet_id, out_packet_data) = client.comm(123, b"Hello!")
    print("id:", out_packet_id)
    print("data:", out_packet_data)