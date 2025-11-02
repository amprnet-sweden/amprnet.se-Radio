/*
 * Copyright (c) 2013, WIZnet Co., Ltd.
 * Copyright (c) 2016, Nicholas Humfrey
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "w5500.h"
#include <string.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/GPIO.h>
#include "ti_drivers_config.h"

//#include <SPI.h>

#define MSGSIZE 1524
SPI_Handle      spi;
SPI_Params      spiParams;
SPI_Transaction spiTransaction;
uint8_t         transmitBuffer[MSGSIZE];
uint8_t         receiveBuffer[MSGSIZE];
bool            transferOK;

#define wizchip_cs_select() GPIO_write(SPICS, 0)
#define wizchip_cs_deselect() GPIO_write(SPICS, 1)

uint8_t _mac_address[6];

//uint8_t Wiznet5500::wizchip_read(uint8_t block, uint16_t address)
uint8_t wizchip_read(uint8_t block, uint16_t address)
{
    uint8_t ret;


    block |= AccessModeRead;

    transmitBuffer[0] = ((address & 0xff00) >> 8);
    transmitBuffer[1] = ((address & 0x00ff) >> 0);
    transmitBuffer[2] = block;
    spiTransaction.txBuf = (void *)transmitBuffer;
    spiTransaction.rxBuf = (void *)receiveBuffer;
    spiTransaction.count = 4;
    wizchip_cs_select();
    transferOK = SPI_transfer(spi, &spiTransaction);
    wizchip_cs_deselect();
    if (!transferOK) {
       // Error in SPI or transfer already in progress.
       while (1);
    }
    ret = receiveBuffer[3];
//    wizchip_cs_deselect();
    return ret;
}

//uint16_t Wiznet5500::wizchip_read_word(uint8_t block, uint16_t address)
uint16_t wizchip_read_word(uint8_t block, uint16_t address)
{
    uint16_t ret;

    wizchip_cs_select();

    block |= AccessModeRead;

    transmitBuffer[0] = ((address & 0xff00) >> 8);
    transmitBuffer[1] = ((address & 0x00ff) >> 0);
    transmitBuffer[2] = block;
    spiTransaction.txBuf = (void *)transmitBuffer;
    spiTransaction.rxBuf = (void *)receiveBuffer;
    spiTransaction.count = 5;
    wizchip_cs_select();
    transferOK = SPI_transfer(spi, &spiTransaction);
    wizchip_cs_deselect();
    if (!transferOK) {
       // Error in SPI or transfer already in progress.
       while (1);
    }
    ret = receiveBuffer[3]<<8 | (receiveBuffer[4]);
    return ret;
}

//void Wiznet5500::wizchip_read_buf(uint8_t block, uint16_t address, uint8_t* pBuf, uint16_t len)
void wizchip_read_buf(uint8_t block, uint16_t address, uint8_t* pBuf, uint16_t len)
{


    block |= AccessModeRead;

    transmitBuffer[0] = ((address & 0xff00) >> 8);
    transmitBuffer[1] = ((address & 0x00ff) >> 0);
    transmitBuffer[2] = block;
    spiTransaction.txBuf = (void *)transmitBuffer;
    spiTransaction.rxBuf = (void *)receiveBuffer;
    spiTransaction.count = 3;       // just read the response to command
    wizchip_cs_select();
      transferOK = SPI_transfer(spi, &spiTransaction);
      if (!transferOK) {
         // Error in SPI or transfer already in progress.
         while (1);
      }
      spiTransaction.rxBuf = (void *)pBuf;
      spiTransaction.count = len;       // just read the response to command
      transferOK = SPI_transfer(spi, &spiTransaction);
      if (!transferOK) {
         // Error in SPI or transfer already in progress.
         while (1);
      }
    wizchip_cs_deselect();

/*    spiTransaction.count = len+3;
    wizchip_cs_select();
      transferOK = SPI_transfer(spi, &spiTransaction);
    wizchip_cs_deselect();
    if (!transferOK) {
       // Error in SPI or transfer already in progress.
       while (1);
    } */
/*    xprint("Len ");
    xprint_int(len);
    xprint("\n"); */
//    memcpy(&pBuf,&receiveBuffer[3],len);
/*    for (int i = 0; i< len;i++) {
       pBuf[i] = receiveBuffer[i+3];
    } */
/*    wizchip_cs_deselect();
    return ret;
    wizchip_spi_write_byte((address & 0xFF00) >> 8);
    wizchip_spi_write_byte((address & 0x00FF) >> 0);
    wizchip_spi_write_byte(block);
    for(i = 0; i < len; i++)
        pBuf[i] = wizchip_spi_read_byte();

    wizchip_cs_deselect(); */
}

//void Wiznet5500::wizchip_write(uint8_t block, uint16_t address, uint8_t wb)
void wizchip_write(uint8_t block, uint16_t address, uint8_t wb)
{

    block |= AccessModeWrite;

    transmitBuffer[0] = ((address & 0xff00) >> 8);
    transmitBuffer[1] = ((address & 0x00ff) >> 0);
    transmitBuffer[2] = block;
    transmitBuffer[3] = wb;
    spiTransaction.txBuf = (void *)transmitBuffer;
    spiTransaction.rxBuf = (void *)receiveBuffer;
    spiTransaction.count = 4;
    wizchip_cs_select();
    transferOK = SPI_transfer(spi, &spiTransaction);
    wizchip_cs_deselect();
    if (!transferOK) {
       // Error in SPI or transfer already in progress.
       while (1);
    }
/*    wizchip_spi_write_byte((address & 0xFF00) >> 8);
    wizchip_spi_write_byte((address & 0x00FF) >> 0);
    wizchip_spi_write_byte(block);
    wizchip_spi_write_byte(wb); */
}

//void Wiznet5500::wizchip_write_word(uint8_t block, uint16_t address, uint16_t word)
    void wizchip_write_word(uint8_t block, uint16_t address, uint16_t word)
{
        block |= AccessModeWrite;

        transmitBuffer[0] = ((address & 0xff00) >> 8);
        transmitBuffer[1] = ((address & 0x00ff) >> 0);
        transmitBuffer[2] = block;
        transmitBuffer[3] = word>>8;
        transmitBuffer[4] = word;
        spiTransaction.txBuf = (void *)transmitBuffer;
        spiTransaction.rxBuf = (void *)receiveBuffer;
        spiTransaction.count = 5;
        wizchip_cs_select();
        transferOK = SPI_transfer(spi, &spiTransaction);
        wizchip_cs_deselect();
        if (!transferOK) {
           // Error in SPI or transfer already in progress.
           while (1);
        }
//    wizchip_write(block, address,   (uint8_t)(word>>8));
//    wizchip_write(block, address+1, (uint8_t) word);
}

//void Wiznet5500::wizchip_write_buf(uint8_t block, uint16_t address, const uint8_t* pBuf, uint16_t len)
void wizchip_write_buf(uint8_t block, uint16_t address, const uint8_t* pBuf, uint16_t len)
{


    block |= AccessModeWrite;

    transmitBuffer[0] = ((address & 0xff00) >> 8);
    transmitBuffer[1] = ((address & 0x00ff) >> 0);
    transmitBuffer[2] = block;
/*    for(int i = 0;i<len; i++) {
        transmitBuffer[i+3] = pBuf[i];
    } */
    spiTransaction.txBuf = (void *)transmitBuffer;
    spiTransaction.rxBuf = (void *)receiveBuffer;
    spiTransaction.count = 3;
    wizchip_cs_select();
    transferOK = SPI_transfer(spi, &spiTransaction);
    if (!transferOK) {
       // Error in SPI or transfer already in progress.
       while (1);
    }
    spiTransaction.txBuf = (void *)pBuf;
    spiTransaction.count = len;
    transferOK = SPI_transfer(spi, &spiTransaction);
    wizchip_cs_deselect();
    if (!transferOK) {
       // Error in SPI or transfer already in progress.
       while (1);
    }
/*    wizchip_spi_write_byte((address & 0xFF00) >> 8);
    wizchip_spi_write_byte((address & 0x00FF) >> 0);
    wizchip_spi_write_byte(block);
    for(i = 0; i < len; i++)
        wizchip_spi_write_byte(pBuf[i]);

    wizchip_cs_deselect(); */
}
uint16_t getSn_RX_RD() {
    return wizchip_read_word(BlockSelectSReg, Sn_RX_RD);
}

void setSn_RX_RD(uint16_t rxrd) {
    wizchip_write_word(BlockSelectSReg, Sn_RX_RD, rxrd);
}
void setSn_TX_WR(uint16_t txwr) {
   wizchip_write_word(BlockSelectSReg, Sn_TX_WR, txwr);
}
uint16_t getSn_TX_WR() {
   return wizchip_read_word(BlockSelectSReg, Sn_TX_WR);
}
uint8_t getSn_IR() {
    return (wizchip_read(BlockSelectSReg, Sn_IR) & 0x1F);
}
void setSn_IR(uint8_t ir) {
           wizchip_write(BlockSelectSReg, Sn_IR, (ir & 0x1F));
     }
void setSn_IMR(uint8_t imr) {
    wizchip_write(BlockSelectSReg, Sn_IMR, (imr & 0x1F));
}

 uint8_t getSn_IMR()     {
    return (wizchip_read(BlockSelectSReg, Sn_IMR) & 0x1F);
}




//void Wiznet5500::setSn_CR(uint8_t cr) {
void setSn_CR(uint8_t cr) {
    // Write the command to the Command Register KOLLAD
    wizchip_write(BlockSelectSReg, Sn_CR, cr);
    // Now wait for the command to complete
    while( wizchip_read(BlockSelectSReg, Sn_CR) );
}

//uint16_t Wiznet5500::getSn_TX_FSR()
 uint16_t getSn_TX_FSR() {
    uint16_t val=0,val1=0;
    do
    {
        val1 = wizchip_read_word(BlockSelectSReg, Sn_TX_FSR);
        if (val1 != 0)
        {
            val = wizchip_read_word(BlockSelectSReg, Sn_TX_FSR);
        }
    } while (val != val1);
    return val;
}

void setSn_RXBUF_SIZE(uint8_t rxbufsize) {
    wizchip_write(BlockSelectSReg, Sn_RXBUF_SIZE, rxbufsize);
}

void setSn_TXBUF_SIZE(uint8_t txbufsize) {
    wizchip_write(BlockSelectSReg, Sn_TXBUF_SIZE, txbufsize);
}
/*
void setSn_TX_WR(uint16_t txwr) {
   wizchip_write_word(BlockSelectSReg, Sn_TX_WR, txwr);
}
*/

//uint16_t Wiznet5500::getSn_RX_RSR()
uint16_t getSn_RX_RSR()
{
    uint16_t val=0,val1=0;
    do
    {
        val1 = wizchip_read_word(BlockSelectSReg, Sn_RX_RSR);
        if (val1 != 0) //if updated / updating
        {
            val = wizchip_read_word(BlockSelectSReg, Sn_RX_RSR);
        }
    } while (val != val1); // until two readings same and not zero
    return val;
}

//void Wiznet5500::wizchip_send_data(const uint8_t *wizdata, uint16_t len)
void wizchip_send_data(const uint8_t *wizdata, uint16_t len)
{
    uint16_t ptr = 0;

    if(len == 0) return;
    ptr = getSn_TX_WR();
    wizchip_write_buf(BlockSelectTxBuf, ptr, wizdata, len);

    ptr += len;

    setSn_TX_WR(ptr);
}

//void Wiznet5500::wizchip_recv_data(uint8_t *wizdata, uint16_t len)
void wizchip_recv_data(uint8_t *wizdata, uint16_t len)
{
    uint16_t ptr;

    if(len == 0) return;
    ptr = getSn_RX_RD();
    wizchip_read_buf(BlockSelectRxBuf, ptr, wizdata, len);
    ptr += len;
    setSn_RX_RD(ptr);
}

//void Wiznet5500::wizchip_recv_ignore(uint16_t len)
void wizchip_recv_ignore(uint16_t len)
{
    uint16_t ptr;

    ptr = getSn_RX_RD();
    ptr += len;
    setSn_RX_RD(ptr);
}
uint8_t getMR()
    {
    return wizchip_read(BlockSelectCReg, MR);
}
void setMR(uint8_t mode)
    {
    wizchip_write(BlockSelectCReg, MR, mode);
    }
void setSHAR(uint8_t* macaddr)
      {
   wizchip_write_buf(BlockSelectCReg, SHAR, macaddr, 6);
}

void getSHAR(uint8_t* macaddr) {
//   wizchip_read_buf(BlockSelectCReg, SHAR, macaddr, 6);
}

uint8_t getPHYCFGR()       {
   return wizchip_read(BlockSelectCReg, PHYCFGR);
}
uint8_t getSIR()       {
   return wizchip_read(BlockSelectCReg, SIR);
}
void setSIR(char c)       {
   wizchip_write(BlockSelectCReg, SIR,c);
}
uint8_t getSIMR()       {
   return wizchip_read(BlockSelectCReg, SIMR);
}

void setSIMR(char c)       {
   wizchip_write(BlockSelectCReg, SIMR,c);
}


//void Wiznet5500::wizchip_sw_reset()
void wizchip_sw_reset()
{
    setMR(MR_RST);
    getMR(); // for delay

    setSHAR(_mac_address);
}

//int8_t Wiznet5500::wizphy_getphylink()
int8_t wizphy_getphylink()
{
    int8_t tmp;
    if(getPHYCFGR() & PHYCFGR_LNK_ON)
        tmp = PHY_LINK_ON;
    else
        tmp = PHY_LINK_OFF;
    return tmp;
}

//int8_t Wiznet5500::wizphy_getphypmode()
int8_t wizphy_getphypmode()
{
    int8_t tmp = 0;
    if(getPHYCFGR() & PHYCFGR_OPMDC_PDOWN)
        tmp = PHY_POWER_DOWN;
    else
        tmp = PHY_POWER_NORM;
    return tmp;
}

//void Wiznet5500::wizphy_reset()
void wizphy_reset()
{
    uint8_t tmp = getPHYCFGR();
    tmp &= PHYCFGR_RST;
    setPHYCFGR(tmp);
    tmp = getPHYCFGR();
    tmp |= ~PHYCFGR_RST;
    setPHYCFGR(tmp);
}

//int8_t Wiznet5500::wizphy_setphypmode(uint8_t pmode)
int8_t wizphy_setphypmode(uint8_t pmode)
{
    uint8_t tmp = 0;
    tmp = getPHYCFGR();
    if((tmp & PHYCFGR_OPMD)== 0) return -1;
    tmp &= ~PHYCFGR_OPMDC_ALLA;
    if( pmode == PHY_POWER_DOWN)
        tmp |= PHYCFGR_OPMDC_PDOWN;
    else
        tmp |= PHYCFGR_OPMDC_ALLA;
    setPHYCFGR(tmp);
    wizphy_reset();
    tmp = getPHYCFGR();
    if( pmode == PHY_POWER_DOWN)
    {
        if(tmp & PHYCFGR_OPMDC_PDOWN) return 0;
    }
    else
    {
        if(tmp & PHYCFGR_OPMDC_ALLA) return 0;
    }
    return -1;
}


/* Wiznet5500(int8_t cs)
{
    _cs = cs;
}
*/
/*
void setSn_RXBUF_SIZE(uint8_t rxbufsize)
        {
    wizchip_write(BlockSelectSReg, Sn_RXBUF_SIZE, rxbufsize);
}
void setSn_TXBUF_SIZE(uint8_t txbufsize)        {
   wizchip_write(BlockSelectSReg, Sn_TXBUF_SIZE, txbufsize);
}
*/
/*
void clearSIRs() { // After a socket IR, SnIR and SIR need to be reset
  for (int i=0;i<8;i++) {
    W5500.writeSnIR(i,0xFF); // Clear socket i interrupt
  }
  W5500.writeSIR(0xFF); // Clear SIR
}

// disable interrupts for all sockets
inline void disableSIRs() {W5500.writeSIMR(0x00);}

// enable interrupts for all sockets
inline void enableSIRs() {W5500.writeSIMR(0xFF);}
*/

void setSn_MR(uint8_t mr)         {
   wizchip_write(BlockSelectSReg, Sn_MR, mr);
}
uint8_t getSn_SR()        {
   return wizchip_read(BlockSelectSReg, Sn_SR);
}


uint8_t w55mac[6];
bool  w5500begin(uint8_t *mac_address)
{
    wizchip_cs_deselect();
    memcpy(_mac_address, mac_address, 6);
    SPI_init();  // Initialize the SPI driver
    SPI_Params_init(&spiParams);  // Initialize SPI parameters
    spiParams.dataSize = 8;       // 8-bit data size
    spiParams.bitRate = 12000000;   // spi baudrate
    spi = SPI_open(CONFIG_SPI_0, &spiParams);
    if (spi == NULL) {
        while (1);  // SPI_open() failed
    }
//    pinMode(_cs, OUTPUT);
    wizchip_cs_deselect();

//    getSHAR(w55mac);
    uint8_t phyr = getPHYCFGR();
    if(phyr == 0xff)
        return false;
    wizchip_sw_reset();

//    getSn_
    // Use the full 16Kb of RAM for Socket 0
    setSn_RXBUF_SIZE(16);
    setSn_TXBUF_SIZE(16);

    // Set our local MAC address
    setSHAR(_mac_address);

    // Open Socket 0 in MACRaw mode
    setSn_MR(Sn_MR_MACRAW);
    setSn_CR(Sn_CR_OPEN);
    if (getSn_SR() != SOCK_MACRAW) {
        // Failed to put socket 0 into MACRaw mode
        return false;
    }

    // Success
    return true;
}

void w5500end()
{
    setSn_CR(Sn_CR_CLOSE);

    // clear all interrupt of the socket
    setSn_IR(0xFF);

    // Wait for socket to change to closed
    while(getSn_SR() != SOCK_CLOSED);
    SPI_close(spi);
}

//uint16_t Wiznet5500::readFrame(uint8_t *buffer, uint16_t bufsize)
uint16_t w5500readFrame(uint8_t *buffer, uint16_t bufsize)
{
    uint16_t len = getSn_RX_RSR();  // get what is received, hang if zero, not good?
//    GPIO_write(sigpin,0);

    if (len > 0)
    {
        uint8_t head[2];
        uint16_t data_len=0;

        wizchip_recv_data(head, 2);
        setSn_CR(Sn_CR_RECV);   ///< Update RX buffer pointer and receive data

        data_len = head[0];
        data_len = (data_len<<8) + head[1];
        data_len -= 2;
        if (data_len > bufsize)
        {
            // Packet is bigger than buffer - drop the packet
            wizchip_recv_ignore(data_len);
            setSn_CR(Sn_CR_RECV); ///< Update RX buffer pointer and receive data
            return 0;
        }
        wizchip_recv_data(buffer, data_len);
        setSn_CR(Sn_CR_RECV); ///< Update RX buffer pointer and receive data

        // Had problems with W5500 MAC address filtering (the Sn_MR_MFEN option)
        // Do it in software instead:
        return data_len;    //GW
/*        if ((buffer[0] & 0x01) || memcmp(&buffer[0], _mac_address, 6) == 0)
        {
            // Addressed to an Ethernet multicast address or our unicast address
            return data_len;
        } else {
            return 0;
        } */
    }
    return 0;
}

uint16_t  w5500sendFrame(uint8_t *buf, uint16_t len)
{
    // Wait for space in the transmit buffer
    while(1)
    {
        uint16_t freesize = getSn_TX_FSR();
        if(getSn_SR() == SOCK_CLOSED) {
            return -1;
        }
        if (len <= freesize) break;
    };

    wizchip_send_data(buf, len);
    setSn_CR(Sn_CR_SEND);

    while(1)
    {
        uint8_t tmp = getSn_IR();
        if (tmp & Sn_IR_SENDOK)
        {
            setSn_IR(Sn_IR_SENDOK);
            // Packet sent ok
            break;
        }
        else if (tmp & Sn_IR_TIMEOUT)
        {
            setSn_IR(Sn_IR_TIMEOUT);
            // There was a timeout
            return -1;
        }
    }

    return len;
}
