//=========================================================================================================
// engine.cpp - Implements the engine that handles the incoming UDP packets
//=========================================================================================================
#include "globals.h"


enum command_t
{
    CMD_INIT_SEQ  = 0,
    CMD_WRITE_REG = 1,
    CMD_READ_REG  = 2    
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
    m_have_most_recent_msg_id = false;

    // Create the queue that other threads will post messages to
    m_event_queue = xQueueCreate(50, sizeof(packet_t));

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
        // Point to the incoming packet buffer
        unsigned char* in = packet.buffer;

        // Fetch the message ID
        uint32_t msg_id = 0;
        msg_id = (msg_id << 8) | *in++;
        msg_id = (msg_id << 8) | *in++;
        msg_id = (msg_id << 8) | *in++;
        msg_id = (msg_id << 8) | *in++;

        // If this is a init-sequence message, forget that we have a most recent message ID
        if (*in == CMD_INIT_SEQ) m_have_most_recent_msg_id = false;

        // If it's the same as our previously received message ID, ignore this message
        if (m_have_most_recent_msg_id && msg_id == m_most_recent_msg_id) continue;

        // This is now our most recent message ID
        m_most_recent_msg_id = msg_id;
        
        // Keep track of the fact that we have a message ID
        m_have_most_recent_msg_id = true;

        // Fetch the command byte
        unsigned char command = *in++;

        printf("Rcvd msg ID %08X, command %i\n", msg_id, command);

        reply(command);

    }

}
//=========================================================================================================



//=========================================================================================================
// reply() - Sends a reply to the host
//=========================================================================================================
unsigned char reply_buffer[1024];
void CEngine::reply(unsigned char command)
{
    // Point to the reply buffer
    unsigned char* out = reply_buffer;

    // Output the message ID
    *out++ = (m_most_recent_msg_id >> 24);
    *out++ = (m_most_recent_msg_id >> 16);
    *out++ = (m_most_recent_msg_id >>  8);
    *out++ = (m_most_recent_msg_id      );

    // Output the command we are responding to
    *out++ = command;

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