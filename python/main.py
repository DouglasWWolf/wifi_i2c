import socket, time
from wifi_i2c import Wifi_I2C, Wifi_I2C_Ex



#===========================================================================
# blinker() - This controls a Microchip MCP23008 port expander on I2C
#             address 0x23.   There is a blue LED tied to pin 7, a green
#             LED tied to pin 6, and a red LED tied to pin 5
#
# This routine sits in a tight loop forever, flashing between red and
# green.   Merry Christmas :-)
#===========================================================================

def blinker():

    DIRECTION = 0x00    # MCP23008 pin direction register
    GPIO      = 0x09    # MCP23008 input/output register

    # Set the I2C address of the device
    device.set_i2c_address(0x23)

    # Set the bottom five bits as inputs, the top 3 as outputs
    device.write_reg(DIRECTION, 0x1F)

    # Just dump out the value of all the registers
    for i in range(0,10):
        val = device.read_reg(i, 1)
        print("Register", i, "has value", val)

    # Sit in a loop blinking the LED forever
    while True:

        # The green LED is tied to pin 6 of the MCP23008.  Turn it on
        device.write_reg(GPIO, (1 << 6))
        time.sleep(.25)

        # Turn just the red LED on.
        device.write_reg(GPIO, (1 << 5))
        time.sleep(.25)


#===========================================================================
# Execution starts here
#===========================================================================
if __name__ == '__main__':
    # Create the object that lets us control the I2C device via WiFi
    device = Wifi_I2C()

    # Start communicating .
    #   1st param = IP address of our WiFi interface
    #   2nd param = IP address that the ESP32 is listening on
    if device.start('192.168.50.196', '192.168.50.229'):
        print("Started!")
    else:
        print("Failed to connect to ESP32")
        quit()


    # Call our blinker routine and report any exceptions that occur
    try:
        blinker()
    except Wifi_I2C_Ex as e:
        print(e.string)
