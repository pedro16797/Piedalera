from network import WLAN, STA_IF
from socket import socket
import time

# Just making our internet connection
wlan = WLAN(STA_IF)

# The pico caches previous connections, which can be problematic, ensure it's not connected already
if wlan.isconnected():
    wlan.disconnect()

# Try to reconnect
wlan.active(True)
wlan.ifconfig(('192.168.3.54', '255.255.255.0', '192.168.3.55', '8.8.8.8'))
wlan.connect('PicoboardServer', '<password>')
max_wait = 10
while max_wait > 0:
    if wlan.status() < 0 or wlan.status() >= 3:
        break
    max_wait -= 1
    time.sleep(1)

# Handle connection error
if wlan.status() != 3:
    raise RuntimeError('Connection failed')
else:
    status = wlan.ifconfig()
    print('Connected with IP' + str(status))

while True:
    # Create a socket and make a HTTP request
    s = socket()
    text = "Testing"
    try:
        s.connect(("192.168.3.55", 80))
        s.send(text)
        reply=str(s.recv(512))
        print(reply)
    except:
        print("Connection failed")
        time.sleep(0.8)
    s.close()
    time.sleep(0.2)