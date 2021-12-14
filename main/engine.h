#pragma once
//=========================================================================================================
// engine.h - Defines the engine that handles the incoming UDP packets
//=========================================================================================================



//=========================================================================================================
// This is the structure of a command packet
//=========================================================================================================
struct command_t
{
    uint8_t tx_en;
    uint8_t prf_sel;
    uint8_t clk_sel;
    uint8_t pt_sel;
    uint8_t seq_loop_cnt;
    uint8_t seq_sel;
    uint8_t tx_dur_msb;
    uint8_t tx_dur_lsb;
};
//=========================================================================================================


class CEngine
{
public:
    // Called once at program startup
    void    init();

    // When a new packet arrives via UDP, this routine gets called
    void    on_incoming_packet(const char* packet, int length);

protected:

    // Sends a status message back to the client side
    void    send_status();

    // A set of bitflags with 1 bit per packet type
    int     m_rcvd_flags;

};
