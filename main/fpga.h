//=========================================================================================================
// fpga.h - Defines the interface to the FPGA
//=========================================================================================================
#pragma once


const int RAM_FRAME_SIZE = 577;


class CFPGA
{
public:

    // Init, called once at startup
    bool    init(int i2c_address);

    // Call this to send a command structure to the FPGA
    void    send_command(command_t& command);

    // Sends a frame of data to the FPGA RAM
    bool    send_packet(int which, const char* data);

    // The version and revision of the FPGA, read during init()
    int     version;
    int     revision;

protected:

    // Call this to write a value to a register
    bool write_reg(int register_address, int value);

    // Call this to read a value from a register
    bool read_reg(int register_address, int* p_result);

    // The I2C address we talk to the FPGA at
    int  m_i2c_address;
};
