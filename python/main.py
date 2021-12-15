import socket, time
from wifi_i2c import Wifi_I2C, Wifi_I2C_Ex

device = Wifi_I2C()

if device.start('192.168.50.196', '192.168.50.229', 0):
    print("Started!")
else:
    print("Failed to connect to ESP32")
    quit()


try:
    device.set_i2c_address(0x23)
    device.write_reg(0, 0x1F)
    device.write_reg(9, 0)
    for i in range(0,10):
        val = device.read_reg(i, 1)

    while True:
        device.write_reg(9, 64)
        time.sleep(.05)
        device.write_reg(9, 0)
        time.sleep(.05)


except Wifi_I2C_Ex as e:
    print(e.string)

quit()

data = bytearray()
frame0 = bytearray()
frame1 = bytearray()
frame2 = bytearray()
frame3 = bytearray()

for i in range(0, 577):
    data.append(i % 256)

frame0.append(1)
frame1.append(2)
frame2.append(3)
frame3.append(4)

frame0.extend(data)
frame0.extend(data)

frame1.extend(data)
frame1.extend(data)

frame2.extend(data)
frame2.extend(data)

frame3.extend(data)
frame3.extend(data)

cmd = bytearray()
for i in range(0, 8):
    cmd.append(0)

status = bytearray()
status.append(5)

p1 = bytearray()
p1.append(0)
p1.append(0)
p1.append(1)
p1.append(2)
p1.append(42)

time.sleep(5)