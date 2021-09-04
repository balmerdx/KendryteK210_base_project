import socketserver

class MyTCPHandler(socketserver.BaseRequestHandler):
    """
    The request handler class for our server.

    It is instantiated once per connection to the server, and must
    override the handle() method to implement communication to the
    client.
    """

    def handle(self):
        # self.request is the TCP socket connected to the client
        self.data = self.request.recv(1024)
        print("{} wrote:".format(self.client_address[0]))
        print("len=", len(self.data))
        print(self.data)
        # just send back the same data, but upper-cased
        if len(self.data)==2:
            out_data = bytearray(1000000)
        else:
            out_data = b"Hello from server!"
        self.request.sendall(out_data)

if __name__ == "__main__":
    HOST, PORT = "192.168.1.48", 5001

    # Create the server, binding to localhost on port 9999
    with socketserver.TCPServer((HOST, PORT), MyTCPHandler) as server:
        # Activate the server; this will keep running until you
        # interrupt the program with Ctrl-C
        server.serve_forever()
