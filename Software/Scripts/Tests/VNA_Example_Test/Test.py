import socket
from asyncio import IncompleteReadError  # only import the exception class

class SocketStreamReader:
    def __init__(self, sock: socket.socket):
        self._sock = sock
        # self._recv_buffer = bytearray()

    def read(self, num_bytes: int = -1) -> bytes:
        raise NotImplementedError

    def readexactly(self, num_bytes: int) -> bytes:
        buf = bytearray(num_bytes)
        # pos = 0
        # while pos < num_bytes:
            # n = self._recv_into(memoryview(buf)[pos:])
            # if n == 0:
                # raise IncompleteReadError(bytes(buf[:pos]), num_bytes)
            # pos += n
        return bytes(buf)

    def readline(self) -> bytes:
        # return self.readuntil(b"\n")
        buf = bytearray(1)
        return bytes(buf)

    def readuntil(self, separator: bytes = b"\n") -> bytes:
        # if len(separator) != 1:
            # raise ValueError("Only separators of length 1 are supported.")

        # chunk = bytearray(4096)
        # start = 0
        # buf = bytearray(len(self._recv_buffer))
        # bytes_read = self._recv_into(memoryview(buf))
        # assert bytes_read == len(buf)

        # while True:
            # idx = buf.find(separator, start)
            # if idx != -1:
                # break

            # start = len(self._recv_buffer)
            # bytes_read = self._recv_into(memoryview(chunk))
            # buf += memoryview(chunk)[:bytes_read]

        # result = bytes(buf[: idx + 1])
        # self._recv_buffer = b"".join(
            # (memoryview(buf)[idx + 1 :], self._recv_buffer)
        # )
        # return result
        buf = bytearray(1)
        return bytes(buf)

    def _recv_into(self, view: memoryview) -> int:
        # bytes_read = min(len(view), len(self._recv_buffer))
        # view[:bytes_read] = self._recv_buffer[:bytes_read]
        # self._recv_buffer = self._recv_buffer[bytes_read:]
        # if bytes_read == len(view):
            # return bytes_read
        # bytes_read += self._sock.recv_into(view[bytes_read:])
        # return bytes_read
        return 0

class Test:
    # By default measure return 2 data points
    measure_nb_points = 2

    def __init__(self, host='localhost', port=19542):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # try:
            # self.sock.connect((host, port))
        # except:
            # raise Exception("Unable to connect to HP8753D. Make sure it is running and the TCP server is enabled.")
        # self.reader = SocketStreamReader(self.sock)

    def __del__(self):
        # self.sock.close()
        return

    def __read_response(self):
        # return self.reader.readline().decode().rstrip()
        return 


    def cmd(self, cmd):
        # self.sock.sendall(cmd.encode())
        # self.sock.send(b"\n")
        # self.__read_response()
        return 

    def query(self, query):
        # self.sock.sendall(query.encode())
        # self.sock.send(b"\n")
        # return self.__read_response()
        return ""

    def measure_set_nb_points(self, nb_points):
        """
        For test purpose set number of points to create during measure()
        """
        self.measure_nb_points = nb_points
        return

    def parse_trace_data(self, data):
        ret = []
        start_freq = 30000.0 # Start freq in Hz
        stop_freq = 6000000000.0 # Stop freq in Hz
        freq_offset = (stop_freq - start_freq) / (self.measure_nb_points - 1)
        freq = start_freq
        # Remove brackets (order of data implicitly known)
        # data = data.replace(']','').replace('[','')
        # values = data.split(',')
        # if int(len(values) / 3) * 3 != len(values):
            # number of values must be a multiple of three (frequency, real, imaginary)
            # raise Exception("Invalid input data: expected tuples of three values each")
        # for i in range(0, len(values), 3):
            # freq = float(values[i])
            # real = float(values[i+1])
            # imag = float(values[i+2])
            # ret.append((freq, complex(real, imag)))
        for i in range(0, self.measure_nb_points):
            freq = freq
            real = float(0.1)
            imag = float(0.2)
            ret.append((freq, complex(real, imag)))
            freq = freq + freq_offset
        return ret
