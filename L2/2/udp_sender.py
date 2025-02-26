import socket
import time

# Completati cu adresa IP a platformei ESP32
PEER_IP = "192.168.89.46"
PEER_PORT = 10001

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
while 1:
    try:
        value = input('Turn on/off the LED (0/1):')
        if value == '0':
            MESSAGE = 'GPIO4=0'
        elif value == '1':
            MESSAGE = 'GPIO4=1'
        else:
            print(f'Invalid input {value}!')
        TO_SEND = bytes(MESSAGE, 'ascii')
        sock.sendto(TO_SEND, (PEER_IP, PEER_PORT))
        print("Message sent: ", TO_SEND)
        time.sleep(1)
    except KeyboardInterrupt:
        break