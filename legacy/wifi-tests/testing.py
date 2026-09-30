import network
import socket

print('Starting...')

WIFI_NAME = 'PicoboardServer'
WIFI_PSWD = '<password>'
IP = '192.168.3.55'

# Create the network and configure it
ap = network.WLAN(network.AP_IF)
ap.active(False)
ap.config(ssid=WIFI_NAME)
ap.config(key=WIFI_PSWD)
ap.ifconfig((IP, '255.255.255.0', IP, '8.8.8.8'))
ap.active(True)
while ap.active() == False:
    pass

# Creating a socket for just 1 device
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('0.0.0.0', 80))
s.listen(1)

print(ap.config('ssid') + ' with IP: ' + ap.ifconfig()[0])

# Listen for connections
while True:
    try:
        cl, addr = s.accept()
        request = cl.recv(512)
        print(request)
        cl.send(b"Reply")
        cl.close()

    except OSError as e:
        cl.close()
        print('connection closed')