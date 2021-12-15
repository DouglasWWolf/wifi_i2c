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
                handle_cmd_write_reg(in, data_length);
                break;

            case CMD_READ_REG:
                handle_cmd_read_reg(in, data_length);
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
//=========================================================================================================
void CEngine::handle_cmd_write_reg(const uint8_t* data, int data_length)
{
    while (data_length > 0)
    {
        // Fetch the register number we're writing to
        int reg = *data++;

        // Fetch the length of the data we're writing to the register
        int reg_length = (data[0] << 8) | data[1];
        data += 2;

        // We have now have three fewer bytes of data in the buffer
        data_length -=3;

        // If there isn't enough data in the buffer to satisfy the register length, something is awry
        if (data_length < reg_length)
        {
            printf("Register 0x%02X needs %i bytes.  Not enough data!\n", reg, data_length);
            reply(ERR_NOT_ENUF_DATA);
            return;
        }

        // If we can't write to the I2C, it's an error
        if (!i2c_write(reg, data, reg_length))
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
//=========================================================================================================
unsigned char read_buffer[1024];
void CEngine::handle_cmd_read_reg(const uint8_t* data, int data_length)
{
    // Fetch the register number we're reading from
    int reg = *data++;

    // Fetch the length of the data we're going to read from the register
    int reg_length = (data[0] << 8) | data[1];
    data += 2;

    // If we can't read from the I2C, it's an error
    if (!i2c_read(reg, read_buffer, reg_length))
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
bool CEngine::i2c_read(int reg, uint8_t* data, int length)
{
    printf("Reading %i bytes from register 0x%02x\n", length, reg);

    for (int i=0; i<length;++i) data[i] = 0x80 + i;

    if (reg == 0x50) return false;

    return true;
}
//=========================================================================================================




//=========================================================================================================
// i2c_write() - Writes data to a device register via I2C
//=========================================================================================================
bool CEngine::i2c_write(int reg, const uint8_t* data, int length)
{
    printf("Writing %i bytes ", length);
    int plen = (length < 5) ? length : 5;
    for (int i=0; i<plen; ++i) printf("[%02X]", data[i]);
    printf(" to register 0x%02X\n", reg);
    
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










#if 0




// We're going to count and display the number of full frame sets we receive
int frame_set_count = 0;


//=========================================================================================================
// This is the structure of a status reply that we will send back to the client side
//=========================================================================================================
struct status_reply_t
{
    uint8_t    packet_type;
    uint8_t    fpga_version;
    uint8_t    fpga_revision;
    uint8_t    cmd_pkt_rcvd;
    uint8_t    pcb0_pkt_rcvd;
    uint8_t    pcb1_pkt_rcvd;
    uint8_t    pcb2_pkt_rcvd;
    uint8_t    pcb3_pkt_rcvd;
};
//=========================================================================================================


//=========================================================================================================
// These are the various sorts of packeets we can be sent
//=========================================================================================================
enum
{
    CMD_HEADER  = 0x00,
    PCB0_HEADER = 0x01,
    PCB1_HEADER = 0x02,
    PCB2_HEADER = 0x03,
    PCB3_HEADER = 0x04,
    STAT_HEADER = 0x05
};

// This is a 1 bit for each packet in a full set of frames
const int FRAME_SET = (1 << CMD_HEADER  ) | (1 << PCB0_HEADER) | (1 << PCB1_HEADER)
                    | (1 << PCB2_HEADER ) | (1 << PCB3_HEADER);
//=========================================================================================================

//=========================================================================================================
// These are the various kinds of packets we can receive
//=========================================================================================================
command_t command_packet;

unsigned char data_packet_0[RAM_FRAME_SIZE * 2];
unsigned char data_packet_1[RAM_FRAME_SIZE * 2];
unsigned char data_packet_2[RAM_FRAME_SIZE * 2];
unsigned char data_packet_3[RAM_FRAME_SIZE * 2];
//=========================================================================================================



//=========================================================================================================
// init() - Called once at program startup
//=========================================================================================================
void CEngine::init()
{
    // Indicate that we have not yet received any packets
    m_rcvd_flags = 0;
}
//=========================================================================================================



//=========================================================================================================
// on_incoming_packet() - Called whenever a UDP packet arrives and needs to be handled
//=========================================================================================================
void CEngine::on_incoming_packet(const char* message, int length)
{
    // Packet type is the first byte of the message, and we bump the pointer to the actual data
    unsigned char packet_type = *message++;

    // Stuff the packet into the correct buffer
    switch (packet_type)
    {
        case CMD_HEADER:
            memcpy(&command_packet, message, sizeof command_packet);
            break;

        case PCB0_HEADER:
            memcpy(data_packet_0, message, sizeof data_packet_0);
            break;

        case PCB1_HEADER:
            memcpy(data_packet_1, message, sizeof data_packet_1);
            break;

        case PCB2_HEADER:
            memcpy(data_packet_2, message, sizeof data_packet_2);
            break;

        case PCB3_HEADER:
            memcpy(data_packet_3, message, sizeof data_packet_3);
            break;

        case STAT_HEADER:
            send_status();
            return;

        default:
            printf(">>>> IGNORING UNKNOWN PACKET! <<<<\n");
            UDPServer.reply(&packet_type, 1);
            return;
    }

    // Keep track of which packets we have received
    m_rcvd_flags |= (1 << packet_type);

    // If we've received a full set of frames...
    if (m_rcvd_flags == FRAME_SET)
    {
        printf("Full set of frame received: %i\n\n", ++frame_set_count);
        m_rcvd_flags = 0;
    }
}
//=========================================================================================================


//=========================================================================================================
// send_status() - Sends a status message back to the client side
//=========================================================================================================
void CEngine::send_status()
{
    // This is the message we're going to send back
    status_reply_t    status;

    // The is a packet type 5
    status.packet_type = 0x05;
    
    // Fill in the FPGA version and revision with what we fetched from the FPGA
    status.fpga_version  = FPGA.version;
    status.fpga_revision = FPGA.revision;

    // Fill in the indicators of which packets we have received
    status.cmd_pkt_rcvd  = (m_rcvd_flags & (1 << CMD_HEADER )) ? 0xFF : 0;
    status.pcb0_pkt_rcvd = (m_rcvd_flags & (1 << PCB0_HEADER)) ? 0xFF : 0;
    status.pcb1_pkt_rcvd = (m_rcvd_flags & (1 << PCB1_HEADER)) ? 0xFF : 0;
    status.pcb2_pkt_rcvd = (m_rcvd_flags & (1 << PCB2_HEADER)) ? 0xFF : 0;
    status.pcb3_pkt_rcvd = (m_rcvd_flags & (1 << PCB3_HEADER)) ? 0xFF : 0;

    // And send this message back to the host
    UDPServer.reply(&status, sizeof status);
}
//=========================================================================================================





#endif