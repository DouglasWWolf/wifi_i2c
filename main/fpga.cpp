//=========================================================================================================
// fpga.cpp - Implements the interface to the FPGA
//=========================================================================================================
#include "globals.h"


#define EXPECTED_FPGA_VER 0xD5
#define EXPECTED_FPGA_REV 0x04


#if 0

//=========================================================================================================
// These are the addresses of the various FPGA registers
//=========================================================================================================
enum
{
    REG_VER             = 0x00,
    REG_REV             = 0x01,
    REG_EN              = 0x04,
    REG_TX_START        = 0x05,
    REG_TX_PT_DUR_MSB   = 0x07,
    REG_TX_PT_DUR_LSB   = 0x08,
    REG_TX_PT_SEL       = 0x25,
    REG_TX_SEQ_LOOP_CNT = 0x26,
    REG_TX_SEQ_SEL      = 0x28,
    REG_CLK_SEL         = 0x29,
    REG_PRF             = 0x2A,
    REG_RAM             = 0x80
};
//=========================================================================================================




//=========================================================================================================
// init() - Called once at startup
//=========================================================================================================
bool CFPGA::init(int i2c_address)
{
    bool status = true;

    // Save our I2C address for posterity
    m_i2c_address = i2c_address;

    // Set both version and revision to 0 in case the following register reads fail
    version = revision = 0;

    // Read both the revision and version registers from the FPGA
    if (!read_reg(REG_REV, &revision)) status = false;
    if (!read_reg(REG_VER, &version )) status = false;


    // Check to see if we got the expected versions 
    if (status)
    {
        if (revision != EXPECTED_FPGA_REV)
            printf("Expected FPGA rev 0x%02X, and got 0x%02X!!\n", EXPECTED_FPGA_REV, revision);

        if (version != EXPECTED_FPGA_VER)
            printf("Expected FPGA ver 0x%02X, and got 0x%02X!!\n", EXPECTED_FPGA_VER, version);
    }

    // Tell the caller whether all is well
    return status;
}
//=========================================================================================================




//=========================================================================================================
// write_reg() - Writes an 8-bit value to one of the registers
//=========================================================================================================
bool CFPGA::write_reg(int register_address, int value)
{
    // Instruct the I2C bus to write the register number and the value to be stored there
    bool status = I2C.write(m_i2c_address, register_address, 1, value, 1);

    // Tell the caller whether or not this write operation was successful
    return status;
}
//=========================================================================================================



//=========================================================================================================
// read_reg() - Reads an 8-bit value from one of the registers
//
// Returns: 'true' if the I2C read operation completed succesfully
//
// On Exit: *p_result = the byte that was read from the specified register
//=========================================================================================================
bool CFPGA::read_reg(int register_address, int* p_result)
{
    uint8_t  data;

    // If the caller gave us a pointer to an result value, fill it in with "the read failed"
    if (p_result) *p_result = -1;

    // Write the address of the register that we want to read, then perform the read operation
    bool status = I2C.write(m_i2c_address, register_address, 1);
    if (status) status = I2C.read(m_i2c_address, &data, 1);

    // If the read/write operation was succesfull, store the register value for the caller
    if (status && p_result) *p_result = data;

    // Tell the caller whether or not this read operation was successful
    return status;
}
//=========================================================================================================



//=========================================================================================================
// send_command() - Sends a command structure to the FPGA
//=========================================================================================================
void CFPGA::send_command(command_t& command)
{
    write_reg(REG_TX_START,        command.tx_en       );
    write_reg(REG_TX_PT_DUR_MSB,   command.tx_dur_msb  );
    write_reg(REG_TX_PT_DUR_LSB,   command.tx_dur_lsb  );
    write_reg(REG_TX_PT_SEL,       command.pt_sel      );
    write_reg(REG_TX_SEQ_LOOP_CNT, command.seq_loop_cnt);
    write_reg(REG_PRF,             command.prf_sel     );
    write_reg(REG_TX_SEQ_SEL,      command.seq_sel     );
    write_reg(REG_CLK_SEL,         command.clk_sel     );
}
//=========================================================================================================



//=========================================================================================================
// send_packet() - Sends an packet (i.e,, 2 frames) of data to the FPGA's RAM
//=========================================================================================================
bool CFPGA::send_packet(int which, const char* data)
{
    return true;

}
//=========================================================================================================
#endif