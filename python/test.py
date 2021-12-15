import socket, time
from wifi_i2c import Wifi_I2C, Wifi_I2C_Ex


def test():

    fwrev = device.get_firmware_rev()
    rssi   = device.get_rssi();
    print("Firmware revision is", fwrev)
    print("Rssi is", rssi)

    DIRECTION = 0x00    # MCP23008 pin direction register
    GPIO      = 0x09    # MCP23008 input/output register

    # Set the I2C address of the device
    device.set_i2c_address(0x00)

    # Just dump out the value of all the registers
    for i in range(0,10):
        reg = i + 10
        val = device.read_reg(reg, 1)
        print("Register", reg, "has value", val)

    device.write_reg([(10, 0x10), (11, 0x11), (12, 0x12), (13, 0x13)])

    print()
    for i in range(0,10):
        reg = i + 10
        val = device.read_reg(reg, 1)
        print("Register", reg, "has value", val)



#===========================================================================
# Execution starts here
#===========================================================================
if __name__ == '__main__':
    router = True

    if router:
        local_ip = "192.168.50.196"
        server_ip = "192.168.50.229"
    else:
        local_ip = None
        server_ip = None

    # Create the object that lets us control the I2C device via WiFi
    device = Wifi_I2C(local_ip)

    # Start communicating .
    #   1st param = IP address of our WiFi interface
    #   2nd param = IP address that the ESP32 is listening on
    if device.start(server_ip):
        print("Started!")
    else:
        print("Failed to connect to ESP32")
        quit()


    # Call our blinker routine and report any exceptions that occur
    try:
        test()
    except Wifi_I2C_Ex as e:
        print(e.string)
