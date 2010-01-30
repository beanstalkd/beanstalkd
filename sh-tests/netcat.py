#!/usr/bin/python
# Usage: netcat.py hostname port

import socket, sys
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((sys.argv[1], int(sys.argv[2])))
s.sendall(sys.stdin.read())
while 1:
    data = s.recv(1024)
    if not data: break
    sys.stdout.write(data)
