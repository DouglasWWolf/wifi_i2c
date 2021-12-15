//=========================================================================================================
// engine.h - Defines the engine that handles the incoming UDP packets
//=========================================================================================================
#pragma once
#include "common.h"


//=========================================================================================================
// This is the data descriptor that describes an incoming packet
//=========================================================================================================
struct packet_t
{
    uint8_t*    buffer;
    uint16_t    length;    
};
//=========================================================================================================


class CEngine
{
public:

    // Called once at program startup to start the thread
    void    begin();

    // Call this to handle an incoming packet
    void    handle_packet(uint8_t* buffer, int length);

public:


    // This is the code that executes in it's own thread
    void    task();

protected:

    // Sends a reply to the most recently received message
    void        reply(int error_code, const uint8_t* data = nullptr, int data_length = 0);
    void        reply(int error_code, int p1);

    // This handles CMD_WRITE_REG commands
    void        handle_cmd_write_reg(const uint8_t* data, int data_length);

    // Call this to write to a device register via I2C
    bool        i2c_write(int reg, const uint8_t* data, int length);

    // If this is true, we have a most recent transaction ID
    bool        m_have_most_recent_trans_id;

    // This is the most recent message we've received
    uint32_t    m_most_recent_trans_id;

    // The command that is currently being handled 
    uint8_t     m_command;
    
    // This is the handle of the currently running server task
    TaskHandle_t m_task_handle;

    // This is the queue that other threads will publish notifications to
    xQueueHandle m_event_queue;
};
