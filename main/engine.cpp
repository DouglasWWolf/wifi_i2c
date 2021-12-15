//=========================================================================================================
// engine.cpp - Implements the engine that handles the incoming UDP packets
//=========================================================================================================
#include "globals.h"


enum command_t
{
    CMD_INIT_SEQ    = 0,
    CMD_CLIENT_PORT = 1,
    CMD_I2C_ADDR    = 2,    
    CMD_WRITE_REG   = 3,
    CMD_READ_REG    = 4
};

enum error_code_t
{
    ERR_NONE          = 0,
    ERR_NOT_ENUF_DATA = 1,
    ERR_I2C_WRITE     = 2,
    ERR_I2C_READ      = 3
};

//=========================================================================================================
// launch_task() - Calls the "task()" routine in the specified object
//
// Passed: *pvParameters points to the object that we want to use to run the task
//=========================================================================================================
static void launch_task(void *pvParameters)
{
    // Fetch a pointer to the object that is going to run out task
    CEngine* p_object = (CEngine*) pvParameters;
    
    // And run the task for that object!
    p_object->task();
}
//=========================================================================================================


//=========================================================================================================
// start() - Starts the UDP server task
//=========================================================================================================
void CEngine::begin()
{
    // We don't have a most recent message ID
    m_have_most_recent_trans_id = false;

    // Create the queue that other threads will post messages to
    m_event_queue = xQueueCreate(50, sizeof(packet_t));

    // A default address for a device on the I2C bus that we'll be talking to
    m_i2c_address = 0x62;

    // And start the task
    xTaskCreatePinnedToCore(launch_task, "i2c_engine", 4096, this, DEFAULT_TASK_PRI, &m_task_handle, TASK_CPU);
}
//=========================================================================================================


//=========================================================================================================
// handle_packet() - Sends a notification of a packet to be handled
//=========================================================================================================
void CEngine::handle_packet(uint8_t* buffer, int length)
{
    packet_t message = {buffer, (uint16_t)length};

    // Stuff this event into the event queue
    xQueueSend(m_event_queue, &message, 0);
}
//=========================================================================================================


//=========================================================================================================
// task() - This is the thread that handles incoming messages
//=========================================================================================================
void CEngine::task()
{
    packet_t    packet;
    
    // Loop forever, waiting for packets to arrive
    while (xQueueReceive(m_event_queue, &packet, portMAX_DELAY))
    {
        // If there's not a transaction ID and command in the packet, ignore it
        if (packet.length < 5) return;

        // Point to the incoming packet buffer
        unsigned char* in = packet.buffer;

        // Fetch transaction ID from this message
        uint32_t trans_id = 0;
        trans_id = (trans_id << 8) | *in++;
        trans_id = (trans_id << 8) | *in++;
        trans_id = (trans_id << 8) | *in++;
        trans_id = (trans_id << 8) | *in++;

        // If this is a init-sequence message, forget that we have a most recent message ID
        if (*in == CMD_INIT_SEQ || *in == CMD_CLIENT_PORT) m_have_most_recent_trans_id = false;

        // If the message we just received as the same transaction ID as the previous one, ignore it
        if (m_have_most_recent_trans_id && trans_id == m_most_recent_trans_id) continue;

        // This is now our most recent transactionge ID
        m_most_recent_trans_id = trans_id;
        
        // Keep track of the fact that we have a transaction ID
        m_have_most_recent_trans_id = true;

        // Fetch the command byte
        m_command = *in++;

        // The transaction ID and command took up 5 bytes.  This is how much is left
        int data_length = packet.length - 5;

        // Handle each type of command we know about
        switch(m_command)
        {
            case CMD_INIT_SEQ:
                reply(0);
                break;
            
            case CMD_WRITE_REG:
                handle_cmd_write_reg(1, in, data_length);
                break;

            case CMD_READ_REG:
                handle_cmd_read_reg(1, in, data_length);
                break;

            case CMD_CLIENT_PORT:
                handle_cmd_client_port(in, data_length);
                break;

            case CMD_I2C_ADDR:
                handle_cmd_i2c_addr(in, data_length);
                break;
        }
    }

}
//=========================================================================================================



//=========================================================================================================
// handle_cmd_write_reg() - Writes data to one or more registers on the I2C device
//
// Passed:  width       = The width, in bytes of a register number
//          data        = Pointer to the buffer full of register-write commands
//          data_length = The number of bytes in the data buffer
//=========================================================================================================
void CEngine::handle_cmd_write_reg(int width, uint8_t* data, int data_length)
{
    int reg, reg_length, w;

    while (data_length > 0)
    {
        reg = reg_length = 0;
        w = width;

        // Fetch the register number we're writing to
        while (w--) reg = (reg << 8) | *data++;

        // Fetch the length of the data we're going to read from the register
        reg_length = (reg_length << 8) | *data++;
        reg_length = (reg_length << 8) | *data++;

        // We have now have fewer bytes of data in the buffer
        data_length = data_length - width - 2;

        // If there isn't enough data in the buffer to satisfy the register length, something is awry
        if (data_length < reg_length)
        {
            printf("Register 0x%02X needs %i bytes.  Not enough data!\n", reg, data_length);
            reply(ERR_NOT_ENUF_DATA);
            return;
        }

        // If we can't write to the I2C, it's an error
        if (!i2c_write(reg, width, data, reg_length))
        {
            reply(ERR_I2C_WRITE, reg);
            return;
        }
        
        // Point to the next register entry in our input packet
        data_length -= reg_length;
        data        += reg_length;
    }

    // Tell the client that everything worked
    reply(ERR_NONE);
}
//=========================================================================================================




//=========================================================================================================
// handle_cmd_write_read() - Writes data to one or more registers on the I2C device
//
// Passed:  width       = The width, in bytes of a register number
//          data        = Pointer to buffer that says how many bytes to read
//          data_length = Length of that buffer (we don't need it)
//=========================================================================================================
unsigned char read_buffer[1024];
void CEngine::handle_cmd_read_reg(int width, const uint8_t* data, int data_length)
{
    int reg = 0, reg_length = 0;

    // Fetch the register number we're reading from
    while (width--) reg = (reg << 8) | *data++;

    // Fetch the length of the data we're going to read from the register
    reg_length = (reg_length << 8) | *data++;
    reg_length = (reg_length << 8) | *data++;

    // If we can't read from the I2C, it's an error
    if (!i2c_read(reg, 1,read_buffer, reg_length))
    {
        reply(ERR_I2C_READ, reg);
        return;
    }

    // Tell the client that everything worked
    reply(ERR_NONE, read_buffer, reg_length);
}
//=========================================================================================================



//=========================================================================================================
// handle_cmd_client_port() - Sets the UDP port for replying to the client
//=========================================================================================================
void CEngine::handle_cmd_client_port(const uint8_t* data, int data_length)
{
    // Fetch the port number
    int udp_port = (data[0] << 8) | data[1];

    // Tell the server what client port to send responses to
    UDPServer.set_client_port(udp_port);

    // Tell the client that everything worked
    reply(ERR_NONE);
}
//=========================================================================================================



//=========================================================================================================
// i2c_addr() - Declares the I2C address of the device we want to talk to
//=========================================================================================================
void CEngine::handle_cmd_i2c_addr(const uint8_t* data, int data_length)
{
    // Set our internal I2C address
    m_i2c_address = *data;

    // Tell the client that everything worked
    reply(ERR_NONE);
}
//=========================================================================================================




//=========================================================================================================
// i2c_read() - Reads data from a device register via I2C
//=========================================================================================================
bool CEngine::i2c_read(int reg, int width, uint8_t* data, int length)
{
    bool status;

    // Write the address of the byte that we wish to read
    status = I2C.write(m_i2c_address, reg, width);

    // If that fails, complain
    if (!status) 
    {
        printf("I2C write failed on register 0x%02X for I2C device 0x%02X\n", reg, m_i2c_address);
        return false;
    }

    // And read the result
    status = I2C.read(m_i2c_address, data, length);

    // If that fails, complain
    if (!status) 
    {
        printf("I2C read failed on register 0x%02X for I2C device 0x%02X\n", reg, m_i2c_address);
        return false;
    }

    // Tell the caller all is well
    return true;
}
//=========================================================================================================




//=========================================================================================================
// i2c_write() - Writes data to a device register via I2C
//=========================================================================================================
bool CEngine::i2c_write(int reg, int width, uint8_t* data, int length)
{
    bool status;

    // Write to the I2C device
    status = I2C.write(m_i2c_address, reg, width, data, length);

    // If that fails, complain
    if (!status) 
    {
        printf("I2C write failed on register 0x%02X for I2C device 0x%02X\n", reg, m_i2c_address);
        return false;
    }

    // Tell the caller all is well
    return true;
}
//=========================================================================================================


//=========================================================================================================
// reply() - Sends a reply to the host
//=========================================================================================================
unsigned char reply_buffer[1024];

void CEngine::reply(int error_code, int p1)
{
    unsigned char data = (unsigned char)p1;
    reply(error_code, &data, 1);
}

void CEngine::reply(int error_code, const uint8_t* data, int data_len)
{
    // Point to the reply buffer
    unsigned char* out = reply_buffer;

    // Output the message ID
    *out++ = (m_most_recent_trans_id >> 24);
    *out++ = (m_most_recent_trans_id >> 16);
    *out++ = (m_most_recent_trans_id >>  8);
    *out++ = (m_most_recent_trans_id      );

    // Output the command we are responding to
    *out++ = m_command;

    // Output the error_code  
    *out++ = error_code;

    // If there's reply data, stuff it into the buffer
    if (data && data_len)
    {
        // Copy the reply data into the buffer
        memcpy(out, data, data_len);

        // Keep track of where the end of the buffer is
        out += data_len;
    }

    // Figure out how long the reply message is
    int length = out - reply_buffer;

    // Send the reply to the client
    UDPServer.reply(reply_buffer, length);
}
//=========================================================================================================





