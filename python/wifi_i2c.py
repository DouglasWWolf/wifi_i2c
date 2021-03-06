"""
To use this class, do this at the top of your Python code:
         from wifi_i2c import Wifi_I2C, Wifi_I2C_Ex

Public API:

    This documentation is somewhat simplified and is applicable to devices where the register numbers
    are all 8-bits wide.   To use 16-bit or 32-bit register numbers, see the "reg_width" parameter in the
    write_reg() and read_reg() API functions


    ----------------------------------------------------------------------------------------------------------
    Constructor of Wifi_I2C(local_ip_address)

    Pass it either the IP address of your local machine, or None if connecting to the device in AP mode
    ---------------------------------------------------------------------------------------------------------
    start(server_ip, server_port)

    Passed: server_ip   = The IP address of the server, or None if connecting in AP mode
            server_port = The port number to connect to, or 0 to use the default (i.e., 1182)

    Returns: True if a connection was established, False if no communication established
    ---------------------------------------------------------------------------------------------------------
    set_i2c_address(address)

    Sets the 7-bit I2C address of the I2C device you want to talk to

    address 0 is a virtual device with 256 1-byte registers

    Returns: nothing
    ---------------------------------------------------------------------------------------------------------
    write_reg(register_number, value)

    Writes an integer value or a bytearray or a byte string to a register

    Returns: nothing
    ---------------------------------------------------------------------------------------------------------
    write_reg([(reg1, val1), (reg2, val2), (reg3, val3), (reg4, val4) <etc>])

    Writes values to multiple registers, all sent in a single packet

    Returns: nothing
    ---------------------------------------------------------------------------------------------------------
    read_reg(register_number)

    Returns: the integer value in the register
    ---------------------------------------------------------------------------------------------------------
    get_firmware_rev()

    Returns: The firmware revision as an integer
    ---------------------------------------------------------------------------------------------------------
    get_rssi()

    Returns: The signal strength as measured by the server
    ---------------------------------------------------------------------------------------------------------

"""

"""
Change History:
==========================================================================================================
 Vers   When       Who  What
---------------------------------------------------------------------------------------------------------
  1000  15-Dec-21  DWW  Initial creation
=========================================================================================================
"""


import threading, time, socket

# ==========================================================================================================
# Exception class for error reporting
# ==========================================================================================================
class Wifi_I2C_Ex(Exception):

    trans_id   = None
    command    = None
    error_code = None
    register   = None
    string     = ""

    ERR_NONE          = 0
    ERR_NOT_ENUF_DATA = 1
    ERR_I2C_WRITE     = 2
    ERR_I2C_READ      = 3
    ERR_CONN_TIMEOUT  = 99
    ERR_UNSUPPORTED   = 255

    def __init__(self, message):

        # If this is a timeout error...
        if type(message) is int and int(message) == -1:
            self.error = self.ERR_CONN_TIMEOUT
            self.string = "Connection timeout: no reply from server"
            return

        self.trans_id = message[0:3]
        self.command = int(message[4])
        self.error_code = int(message[5])

        # If there are at least 7 bytes in the message, assume the 7th byte is a register
        if len(message) > 6:
            self.register = int(message[6])

        if self.error_code == self.ERR_NONE:
            self.string = "No error"
            return

        if self.error_code == self.ERR_NOT_ENUF_DATA:
            self.string = ("On register %i, not enough data" % self.register)
            return

        if self.error_code == self.ERR_I2C_WRITE:
            self.string = ("On register %i, I2C write error" % self.register)
            return

        if self.error_code == self.ERR_I2C_READ:
            self.string = ("On register %i, I2C read error" % self.register)
            return

        if self.error_code == self.ERR_UNSUPPORTED:
            self.string = ("Unsupported command %i" % self.command)
            return

        self.string = "Unknown error: "+str(self.error_code)
# ==========================================================================================================




# ==========================================================================================================
# wifi_i2c - Manages communications with ESP32 wifi-i2c server
# ==========================================================================================================
class Wifi_I2C:

    # This is the transaction ID of the next message we will send
    transaction_id = 0

    # This is the server address and port
    server = ('', 0)

    # This will be a Listener object
    listener = None

    # This is the socket we'll be transmitting on
    sock = None

    # These are all of the commands we can send to the server
    INIT_SEQ_CMD     = 0
    CLIENT_PORT_CMD  = 1
    I2C_ADDR_CMD     = 2
    WRITE_REG_CMD    = 3
    READ_REG_CMD     = 4
    GET_FWREV_CMD    = 5
    GET_RSSI_CMD     = 6


    # ------------------------------------------------------------------------------------------------------
    # The constructor - Sets up our listening socket
    # ------------------------------------------------------------------------------------------------------
    def __init__(self, local_ip):

        # Create a listener object and start the listening thread
        self.listener = Listener(local_ip)
    # ------------------------------------------------------------------------------------------------------



    # ------------------------------------------------------------------------------------------------------
    # start() - Create sockets and starts the thread that listens for incoming messages
    #
    # Returns:  True if communication was established
    #           False if something goes awry
    # ------------------------------------------------------------------------------------------------------
    def start(self, server_ip = None, server_port = 0):

        # If no IP address was provided, assume we're connecing in AP mode
        if server_ip == None: server_ip = '192.168.4.1'

        # If this listener was unable to bind to a socket, there's nothing we can do
        if self.listener.port == None: return False;

        # If either of the port numbers is 0, use defaults
        if server_port == 0: server_port = 1182

        # Save our server information
        self.server = (server_ip, server_port)

        # Create a socket for sending messages
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        # Tell the server what local port to send responses to
        reply = None
        try:
            reply = self.set_client_port(self.listener.port)
        except Exception:
            return False

        # If we get here, we have communication with our Wi-Fi device!
        return True
    # ------------------------------------------------------------------------------------------------------


    # ------------------------------------------------------------------------------------------------------
    # set_i2c_address() - Tells the server the I2C address of the device to talk to
    # ------------------------------------------------------------------------------------------------------
    def set_i2c_address(self, address):

        # Convert the address to a byte
        address = address.to_bytes(1, 'big')

        # Send the command to the server
        return self.send_message(self.I2C_ADDR_CMD, address)
    # ------------------------------------------------------------------------------------------------------



    # ------------------------------------------------------------------------------------------------------
    # write_reg() - Writes values to a register on the I2C device
    #
    # register_list is an int and value is an int or byte-string
    #
    # register_list can be:
    #   [(register, value), (register, value), (register, value) (etc)]
    # ------------------------------------------------------------------------------------------------------
    def write_reg(self, register_list, value = None, *, reg_width = 1):

        # Get the register data as a stream of bytes
        data = self.build_register_data(register_list, value, reg_width=reg_width)

        # Send the command to the server
        return self.send_message(self.WRITE_REG_CMD, data)
    # ------------------------------------------------------------------------------------------------------


    # ------------------------------------------------------------------------------------------------------
    # read_reg() - Reads a value from a register
    #
    # Returns: The integer contents of the specified register
    # ------------------------------------------------------------------------------------------------------
    def read_reg(self, register, length = 1, *, reg_width = 1):

        # Get register as one or more bytes
        register = register.to_bytes(reg_width, 'big')

        # Convert register-width to a byte
        reg_width = reg_width.to_bytes(1, 'big')

        # Get length as a pair of bytes
        length = length.to_bytes(2, 'big')

        # Send the command to the server
        rc = self.send_message(self.READ_REG_CMD, reg_width + register + length)

        # Convert the value to an integer and hand it to the caller
        return int.from_bytes(rc, 'big')
    # ------------------------------------------------------------------------------------------------------



    # ------------------------------------------------------------------------------------------------------
    # get_firmware_rev() - Fetches and returns the server firmware revision
    # ------------------------------------------------------------------------------------------------------
    def get_firmware_rev(self):

        # Send the request to the server
        rc = self.send_message(self.GET_FWREV_CMD)

        # Convert the value to an integer and hand it to the caller
        return int.from_bytes(rc, 'big')
    # ------------------------------------------------------------------------------------------------------


    # ------------------------------------------------------------------------------------------------------
    # get_rssi() - Fetches the Wi-Fi signal strength that the server sees
    # ------------------------------------------------------------------------------------------------------
    def get_rssi(self):

        # Send the request to the server
        rc = self.send_message(self.GET_RSSI_CMD)

        # Convert the value to an integer and hand it to the caller
        return int.from_bytes(rc, 'big', signed = True)
    # ------------------------------------------------------------------------------------------------------



    # <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
    # <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
    # From here on down are methods that are private to this class
    # <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
    # <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>



    # ------------------------------------------------------------------------------------------------------
    # set_client_port() - Tells the server which UDP port to send responses to
    # ------------------------------------------------------------------------------------------------------
    def set_client_port(self, port):

        # Convert port number to bytes
        port = port.to_bytes(2, 'big')

        # Send the command to the server
        return self.send_message(self.CLIENT_PORT_CMD, port)
    # ------------------------------------------------------------------------------------------------------



    # ------------------------------------------------------------------------------------------------------
    # send_message() - Sends a message to the server, making multiple attempts to get a reply
    #
    # Returns: response bytes
    #   or None = Transaction was good, but no response data
    # ------------------------------------------------------------------------------------------------------
    def send_message(self, command, data = None):

        # Increment our outgoing transaction ID so the server knows this is a new msg
        self.transaction_id = self.transaction_id + 1

        # Get the bytes for the transaction ID
        id = self.transaction_id.to_bytes(4, 'big')

        # Build the message we're about to send'
        message = id
        message = message + command.to_bytes(1, 'big')

        # If there is data to go with the message, append it
        if data:
            if type(data) is bytearray:
                data = bytes(data)
            if not type(data) is bytes:
                raise TypeError("send_message: data must be bytes, not "+ str(type(data)))
            message = message + data

        # So far we don't have a reply message
        reply = None

        # We're going to make five attempts to get a reply
        for attempt in range(0, 5):

            # Tell the listener to expect this message ID
            self.listener.expect(id)

            # Send the message to the server
            self.sock.sendto(message, self.server)

            # Fetch the reply
            reply = self.listener.wait_for_reply(1)

            # If we have a reply, we don't have to retry
            if reply: break

        # If we didn't receive a reply, that's an error
        if not reply: raise Wifi_I2C_Ex(-1)


        # If there's an error code in the reply raise an exception
        if reply[5] != 0: raise Wifi_I2C_Ex(reply)


        # Tell the caller what reply we got.  A message is:
        #   4 bytes of transaction ID
        #   1 byte of command
        #   1 byte of error code
        #   optional data

        # If there's no reply data, return None
        if len(reply) <= 6: return None

        # Otherwise, return the reply data
        return reply[6:]
    # ------------------------------------------------------------------------------------------------------


    # ------------------------------------------------------------------------------------------------------
    # build_register_data() - Returns a string of bytes built from an input
    #
    # register_list is an int and value is an int or byte-string
    #
    # register_list can be:
    #   [(register, value), (register, value), (register, value) (etc)]
    # ------------------------------------------------------------------------------------------------------
    def build_register_data(self, register_list, value = None, *, reg_width=1):

        data = bytearray()

        # If register is a list of values...
        if type(register_list) is list:

            # Loop through each tuple in the list of values
            for register, value in register_list:
                data = data + self.build_one_register_string(register, value, reg_width)

            # We built a byte string from a list of tuples.  Return it
            return bytes(data)

        # Otherwise, build the string for the one single register number the caller provided
        return self.build_one_register_string(register_list, value, reg_width)
    # ------------------------------------------------------------------------------------------------------


    # ------------------------------------------------------------------------------------------------------
    # build_one_register_string() - Builds the register string for a single register number and value
    # ------------------------------------------------------------------------------------------------------
    def build_one_register_string(self, reg_number, value, reg_width):

        # if value is an int, convert it to one or more bytes
        if type(value) is int:
            value = value.to_bytes(reg_width, 'big')

        # If value is a bytearray, convert it to a bytes string
        if type(value) is bytearray:
            value = bytes(value)

        # If value is a byte string, build our return value from it
        if type(value) is bytes:
            value_length = len(value).to_bytes(2, 'big')

            # Convert the register number to one or more bytes
            reg_number= reg_number.to_bytes(reg_width, 'big')

            # Convert register width to one byte
            reg_width = reg_width.to_bytes(1, 'big')

            # Hand the caller the resulting byte string
            return reg_width + reg_number + value_length + value

        # We'll get here if 'value' wasn't an integer or byte string
        raise TypeError("register values must be int or bytes")
    # ------------------------------------------------------------------------------------------------------



# ==========================================================================================================





# ==========================================================================================================
# listener - A helper thread that listens for reply packets
# ==========================================================================================================
class Listener(threading.Thread):

    sock        = None
    port        = None
    expected_id = None
    event       = None
    incoming    = None

    def __init__(self, local_ip=None):

        # If no IP address was provided, assume we're connecing in AP mode
        if local_ip == None: local_ip = '192.168.4.2'

        # Call the threading base class constructor
        threading.Thread.__init__(self)

        # Create a socket to listen on for incoming messages
        self.sock = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)

        # Bind the listening socket and start the listening thread
        for i in range(0, 500):
            self.port = 30000 + i
            try:
                self.sock.bind((local_ip, self.port))
                self.begin()
                return
            except OSError:
                pass

        # Tell the outside world that we were unable to bind to a port
        self.port = None

        # Visibly complain that we can't find any port we can bind to!
        print("Can't find any available port to bind to")
    # ------------------------------------------------------------------------------------------------------




    # ---------------------------------------------------------------------------
    # begin() - Starts a thread and begins listening for incoming messages
    # ---------------------------------------------------------------------------
    def begin(self):

        # Ensure that this thread exits when the main program does
        self.daemon = True

        # Create an event that other threads can wait on
        self.event = threading.Event()

        # Start the thread
        self.start()
    # ---------------------------------------------------------------------------


    # ---------------------------------------------------------------------------
    # expect() - Tells the listening engine to expect an incoming message
    # ---------------------------------------------------------------------------
    def expect(self, transaction_id):

        # Clear the event.  It will be set if a message arrives
        self.event.clear()

        # Save the message ID that we are expecting
        self.expected_id = transaction_id
    # ---------------------------------------------------------------------------


    # ---------------------------------------------------------------------------
    # wait_for_reply() - Waits for a reply to a message
    #
    # Returns either None or a byte-string
    # ---------------------------------------------------------------------------
    def wait_for_reply(self, seconds):

        # If a reply never arrived, tell the caller
        if not self.event.wait(seconds): return None

        # A message arrived.  Hand it to the caller
        return self.incoming
    # ---------------------------------------------------------------------------



    # ---------------------------------------------------------------------------
    # run() - A blocking thread that permanently waits for incoming messages
    # ---------------------------------------------------------------------------
    def run(self):

        # We're going to wait for incoming messages forever
        while True:

            # Wait for an incoming message
            message, _ = self.sock.recvfrom(1024)

            # What's the message ID of this message?
            trans_id = message[0:4]

            # If this was an unexpected transaction ID, ignore it
            if trans_id != self.expected_id: continue

            # We are no longer expecting a message
            self.expected_id = None

            # Save the incoming message so the other thread can retrieve it
            self.incoming = message

            # Tell the other thread that his reply arrived
            self.event.set()
    # ---------------------------------------------------------------------------

# ==========================================================================================================




