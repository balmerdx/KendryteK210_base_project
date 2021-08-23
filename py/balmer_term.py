import os
import time
import serial
import datetime
import sys
import struct
import argparse
import fcntl
import selectors

#интересный сэмпл https://stackoverflow.com/questions/21791621/taking-input-from-sys-stdin-non-blocking
class BalmerTerminal:
    def BalmerTerminal(self):
        self.ser = None
        pass
    def connect(self, port = '/dev/ttyUSB0', baudrate = 115200):
        self.ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            bytesize=serial.EIGHTBITS,
            timeout = 0.2
        )
        return self.ser.isOpen()
    def close(self):
        self.ser.close()
        self.ser = None

    def read_all(self):
        all_data = b''
        time.sleep(2e-3)
        all_data = self.ser.read(1000)
        return all_data

    def set_input_nonblocking(self):
        orig_fl = fcntl.fcntl(sys.stdin, fcntl.F_GETFL)
        fcntl.fcntl(sys.stdin, fcntl.F_SETFL, orig_fl | os.O_NONBLOCK)

    def from_keyboard(self, arg1):
        line = arg1.read()
        if line == 'quit\n':
            quit()
        else:
            print('User input: {}'.format(line))
            self.ser.write(line.encode('utf-8'))

    def from_uart(self, ser):
        data = self.ser.read(1024)
        print("ser=", data)
        pass

    def terminal(self, port = '/dev/ttyUSB0', baudrate = 115200):
        self.set_input_nonblocking()
        if not self.connect(port, baudrate):
            print(f"Cannot open port={port}",file=sys.stderr)
            return False

        self.m_selector = selectors.DefaultSelector()
        self.m_sel;ector.register(self.ser, selectors.EVENT_READ, lambda arg1: self.from_uart(arg1))
        self.m_selector.register(sys.stdin, selectors.EVENT_READ, lambda arg1: self.from_keyboard(arg1))
        
        while True:
            for k, mask in self.m_selector.select():
                k.data(k.fileobj)
            pass
    pass

if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    port = '/dev/ttyUSB0'
    baudrate = 115200
    parser.add_argument("-p", "--port", help=f"Communication port. Default {port}",
                    type=str, default=port)
    parser.add_argument("-b", "--baudrate", help=f"Baudrate. Default {baudrate}",
                    type=int, default=baudrate)
    args = parser.parse_args()
    port = args.port
    baudrate = args.baudrate
    print("port=", port)
    print("baudrate=", baudrate)
    term = BalmerTerminal()
    term.terminal(port=port, baudrate=baudrate)
    print("Hello!", end='\r')
    time.sleep(1)
    print("Hello!2", end='\r')
    time.sleep(1)
    print("Hello!3", end='\n')
    time.sleep(1)