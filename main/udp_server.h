//=========================================================================================================
// udp_server.h - Defines a UDP message server task
//=========================================================================================================
#pragma once
#include "common.h"

class CUDPServer
{
public:

    CUDPServer() {m_is_running = false;}

    // Call this to start the server task
    void    begin();

    // Call this to stop the server task
    void    stop();

    // Use this to reply back to the host that sent data
    void    reply(void* data, int length);

    // Call this to determine what client port to send replies to
    void    set_client_port(int port) {m_client_port = port;}

public:
    
    // This is the task that serves as our UDP server.
    void    task();

protected:

    // This is the handle of the currently running server task
    TaskHandle_t m_task_handle;

    // This will be 'true' when the server is running
    bool    m_is_running;

    // This is the port number we'll send messages to the client on
    int     m_client_port;
};