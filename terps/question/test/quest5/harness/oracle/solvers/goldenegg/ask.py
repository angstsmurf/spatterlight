"""Send a snippet to serve.py: ask.py PORT 'code' | ask.py PORT -f file.py"""
import socket, sys

code = open(sys.argv[3]).read() if sys.argv[2] == "-f" else sys.argv[2]
c = socket.socket()
c.connect(("127.0.0.1", int(sys.argv[1])))
c.sendall(code.encode() + b"\0")
data = b""
while not data.endswith(b"\0"):
    chunk = c.recv(65536)
    if not chunk:
        break
    data += chunk
sys.stdout.write(data.rstrip(b"\0").decode())
