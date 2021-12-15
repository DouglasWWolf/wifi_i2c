import threading, time, socket

# ==========================================================================================================
# wifi_i2c - Manages communications with ESP32 wifi-i2c server
# ==========================================================================================================
class Wifi_I2C:

    # This is the ID of the next message we will send
    message_id = 0

    # This is the server address and port
    server = ('', 0)

    # This will be a Listener object
    listener = None

    # These are all of the commands we can send to the server
    INIT_SEQ_CMD  = 0
    WRITE_REG_CMD = 1

    # ------------------------------------------------------------------------------------------------------
    # start() - Create sockets and starts the thread that listens for incoming messages
    # ------------------------------------------------------------------------------------------------------
    def start(self, local_ip, local_port, server_ip, server_port):

        # If either of the port numbers is 0, use defaults
        if server_port == 0: server_port = 1182
        if local_port == 0: local_port = server_port

        # Save our server information
        self.server = (server_ip, server_port)

        # Create a listener object
        self.listener = Listener()

        # Create a socket for the listener to use
        self.listener.sock = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)

        # Bind the listening socket
        try:
            self.listener.sock.bind((local_ip, local_port))
        except OSError:
            print("Port %i is locked by some other program" % local_port)
            return False

        # Tell the listener object to start listening
        self.listener.begin()

        # Create a socket for sending messages
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        # Send the "Start a new connection" message, and wait for the reply
        return self.send_message(self.INIT_SEQ_CMD)
    # ------------------------------------------------------------------------------------------------------


    # ------------------------------------------------------------------------------------------------------
    # write_reg() - Writes values to a register on the I2C device
    #
    # register_list is an int and value is an int or byte-string
    #
    # register_list can be:
    #   [(register, value), (register, value), (register, value) (etc)]
    # ------------------------------------------------------------------------------------------------------
    def write_reg(self, register_list, value):

        # Get the register data as a stream of bytes
        data = self.build_register_data(register_list, value)

        # Send the command to the server
        return self.send_message(self.WRITE_REG_CMD, data)
    # ------------------------------------------------------------------------------------------------------




    # <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
    # From here on down are methods that are private to this class
    # <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>

    # ------------------------------------------------------------------------------------------------------
    # send_message() - Sends a message to the server, making multiple attempts to get a reply
    # ------------------------------------------------------------------------------------------------------
    def send_message(self, command, data = None):

        # Increment our outgoing message ID so the server knows this is a new msg
        self.message_id = self.message_id + 1

        # Get the bytes for the message ID
        id = self.message_id.to_bytes(4, 'big')

        # Build the message we're about to send'
        message = id
        message = message + command.to_bytes(1, 'big')

        # If there is data to go with the message, append it
        if data:
            if not type(data) is bytes:
                raise TypeError("send_message: data must be bytes")
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

            if reply: break

        # Tell the caller what reply we got
        return reply
    # ------------------------------------------------------------------------------------------------------

    # ------------------------------------------------------------------------------------------------------
    # build_register_data() - Returns a string of bytes built from an input
    #
    # register_list is an int and value is an int or byte-string
    #
    # register_list can be:
    #   [(register, value), (register, value), (register, value) (etc)]
    # ------------------------------------------------------------------------------------------------------
    def build_register_data(self, register_list, value = None):

        data = bytearray()

        # If register is a list of values...
        if type(register_list) is list:

            # Loop through each tuple in the list of values
            for register, value in register_list:

                # If value is an integer, convert it to bytes
                if type(value) is int:
                    value = value.to_bytes(1, "big")

                # Values must be integers or bytes
                if not type(value) is bytes:
                    raise TypeError("register values must be int or bytes")

                # Find out how many bytes long 'value' is
                value_length = len(value).to_bytes(2, 'big')

                # Append this register definition to our data string
                data = data + register.to_bytes(1, 'big') + value_length + value

            # We built a byte string from a list of tuples.  Return it
            return data


        # if value is an int, convert it to a byte
        if type(value) is int:
            value = value.to_bytes(1, 'big');

        # If value is a byte string, we're done
        if type(value) is bytes:
            value_length = len(value).to_bytes(2, 'big')
            return register_list.to_bytes(1, 'big') + value_length + value

        # We'll get here if 'value' wasn't an integer or byte string
        raise TypeError("register values must be int or bytes")
    # ------------------------------------------------------------------------------------------------------



# ==========================================================================================================





# ==========================================================================================================
# listener - A helper thread that listens for reply packets
# ==========================================================================================================
class Listener(threading.Thread):

    sock = None

    def __init__(self):
        threading.Thread.__init__(self)

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
    def expect(self, message_id):

        # Clear the event.  It will be set if a message arrives
        self.event.clear()

        # Save the message ID that we are expecting
        self.expected_id = message_id
    # ---------------------------------------------------------------------------


    # ---------------------------------------------------------------------------
    # wait_for_reply() - Waits for a reply to a message
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
            msg_id = message[0:4]

            # If this was an unexpected message ID, ignore it
            if msg_id != self.expected_id: continue

            # We are no longer expecting a message
            self.expected_id = None

            # Save the incoming message (sans ID) so the other thread can retrieve it
            self.incoming = message[4:]

            # Tell the other thread that his reply arrived
            self.event.set()
    # ---------------------------------------------------------------------------

# ==========================================================================================================




