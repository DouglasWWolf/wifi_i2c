import socket, time
from wifi_i2c import Wifi_I2C, Wifi_I2C_Ex

fpga = Wifi_I2C()

rc = fpga.start('192.168.50.196', 12345, '192.168.50.229', 0)
if rc:
    print("Started!")
else:
    print("Failed!")
    quit()

print("Sending register")

try:
    fpga.write_reg(0x15, 12)
    config = [(0x10, 10), (0x20, 20), (0x30, 30), (0x40, 40)]
    fpga.write_reg(config)

    val = fpga.read_reg(0x10);
    print("Read from 0x10", val)

    val = fpga.read_reg(50)
    print("Read from 0x50", val)



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