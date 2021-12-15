
//=========================================================================================================
// udp_server.cpp - Implements a UDP message server task
//=========================================================================================================
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/param.h>
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "esp_system.h"

#include "udp_server.h"
#include "common.h"
#include "parser.h"
#include "globals.h"
#include "history.h"

// An identifier that is used by the logging facility
static const char *TAG = "udp_server";

// This is the UDP port number that we're listening on
const int SERVER_PORT = 1182;

// This is contains the information about the server socket
static struct sockaddr_in6 sock_desc;

// When we receive a UDP message, this will contain the IP address of the sender
static struct sockaddr_in6 source_addr; 

// We're going to treat "sock_desc" as though it were a "sock_addr_in"
static sockaddr_in& sockaddr_source = *(sockaddr_in*)&source_addr;

// This contains the socket descriptor of the socket when it's open
static int  sock = -1;

// We expect an incoming message to be no larger than this
#define MAX_PACKET_SIZE 1024

// This is a buffer of all incoming messages
static unsigned char rolling_buffer[20000];

//========================================================================================================= 
// hard_shutdown() - Ensures that the listening socket and the server socket are closed
//========================================================================================================= 
static void hard_shutdown()
{
    // Shut down the server socket
    if (sock != -1)
    {
        shutdown(sock, 0);
        close(sock);
        sock = -1;
    }
}
//========================================================================================================= 



//========================================================================================================= 
//  task - provides a continuously running UDP server that listens for messages and responds to them.
//========================================================================================================= 
void CUDPServer::task()
{
    char source_ip[20];

    // How long is the buffer that will hold the address of the sender?
    socklen_t source_length = sizeof(source_addr);
    
    // We're going to treat "sock_desc" as though it were a "sock_addr_in"
    sockaddr_in& sockaddr = *(sockaddr_in*)&sock_desc;

    // Configure the socket descriptor
    sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(SERVER_PORT);

    // Create the socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return;
    }

    // Bind the socket to our interface using the socket descriptor we created
    int err = bind(sock, (struct sockaddr *)&sock_desc, sizeof(sock_desc));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        return;
    }

    // The next incoming packet shold be placed at the start of the rolling buffer
    unsigned char* next_in = rolling_buffer;

    // This is how many bytes we have free in the buffer
    uint32_t buffer_remaining = sizeof(rolling_buffer);

    // Tell the engineer that the socket is built and we're ready for incoming data
    printf(">>>> Waiting for incoming UDP messages <<<<\n");
    while (true)
    {
        // Wait for a message to arrive
        int length = recvfrom(sock, next_in, buffer_remaining, 0, (struct sockaddr *)&source_addr, &source_length);

        // If that failed, tell the engineer
        if (length < 0)
        {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            continue;
        }

        // Convert the sender's IP address into a string
        inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, source_ip, sizeof(source_ip) - 1);
        
        // Tell the engineer pertinent details about the packet we just received
        printf("Rcvd %4i bytes from %s\n", length, source_ip);

        // Hand this packet to the I2C engine
        Engine.handle_packet(next_in, length);

        // Point to where the next incoming message gets stored
        next_in += length;

        // We now have fewer free bytes in the buffer
        buffer_remaining -= length;

        // If there's not enough room left in the buffer for an entire packet, wrap-around
        // to the start of the buffer
        if (buffer_remaining < MAX_PACKET_SIZE)
        {
            next_in = rolling_buffer;
            buffer_remaining = sizeof rolling_buffer;
        }
    }
}
//========================================================================================================= 



//=========================================================================================================
// launch_task() - Calls the "task()" routine in the specified object
//
// Passed: *pvParameters points to the object that we want to use to run the task
//=========================================================================================================
static void launch_task(void *pvParameters)
{
    // Fetch a pointer to the object that is going to run out task
    CUDPServer* p_object = (CUDPServer*) pvParameters;
    
    // And run the task for that object!
    p_object->task();
}
//=========================================================================================================



//=========================================================================================================
// start() - Starts the UDP server task
//=========================================================================================================
void CUDPServer::begin()
{
    xTaskCreatePinnedToCore(launch_task, "udp_server", 4096, this, TASK_PRIO_UDP, &m_task_handle, TASK_CPU);

    // The server is running!
    m_is_running = true;
}
//=========================================================================================================



//=========================================================================================================
// stop() - Stops the UDP server task
//=========================================================================================================
void CUDPServer::stop()
{
    // Kill the task if it's running
    if (m_is_running)
    {
        vTaskDelete(m_task_handle);
        m_is_running = false;
    }

    // And close the socket if it's open
    hard_shutdown();
}
//=========================================================================================================


//=========================================================================================================
// reply() - Send a reply back to the last host that sent us a message
//=========================================================================================================
void CUDPServer::reply(void* data, int length)
{
    // We want to send our reply back to the same port number we're listening on
    sockaddr_source.sin_port = htons(SERVER_PORT);

    printf("Sending reply\n");

    // Send the message back
    int err = sendto(sock, data, length, 0, (struct sockaddr *)&source_addr, sizeof source_addr);

    // Say something if an error occurs
    if (err < 0) printf("sendto() failed with %i\n", err);
}
//=========================================================================================================
