/*
 * Copyright (c) 2019, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Copyright to the modifications by Gullik Webjörn, SM4FBD, Emma Sviestins, SA0EMY */

/***** Includes *****/

/* Standard C Libraries */
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>


/* TI Drivers */
#include <ti/drivers/rf/RF.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/NVS.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/Timer.h>

/* RTOS header files */
#include <FreeRTOS.h>
#include <task.h>

Timer_Handle    Timhandle;
Timer_Params    Timparams;

/* Driverlib Header files */
#include DeviceFamily_constructPath(driverlib/rf_prop_mailbox.h)
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)

/* Board Header files */
#include "ti_drivers_config.h"

/* Application Header files */
#include "RFQueue.h"
#include "Ampr-radio.h"
#include "Ampr-queue.h"
#include <ti_radio_config.h>

/* ======== uart2 ======== */
#include <ti/drivers/UART2.h>
#include <ti/drivers/Temperature.h>
#include <ti/devices/cc13x1_cc26x1/driverlib/aon_batmon.h>
#include <ti/devices/cc13x1_cc26x1/driverlib/ioc.h>
/***** Defines *****/
#define HAM23CMRADIO 1
//#define FSK4 1
//#define SYN4 1
/* Packet RX Configuration */
#define DATA_ENTRY_HEADER_SIZE 8  /* Constant header size of a Generic Data Entry */
//#define MAX_LENGTH             64 /* Max length byte the radio will accept */
#define MAX_LENGTH             255 /* Max length byte the radio will accept if one size byte */
#define NUM_DATA_ENTRIES       32  /* NOTE: Only two data entries supported at the moment */
#define NUM_APPENDED_BYTES     2  /* The Data Entries data field will contain:
                                   * 1 Header byte (RF_cmdPropRx.rxConf.bIncludeHdr = 0x1)
                                   * Max 30 payload bytes
                                   * 1 status byte (RF_cmdPropRx.rxConf.bAppendStatus = 0x1) */
#define NO_PACKET              0
#define PACKET_RECEIVED        1
#define Nsize 64                // size of command buffer and get parameter buffer

/*******Global variable declarations*********/
static RF_Object rfObject;
static RF_Handle rfHandle;

RF_CmdHandle rfPostHandle;

UART2_Handle uart;
UART2_Params uartParams;


// nvram
//#define NVS_REGIONS_BASE 0x48000
NVS_Handle nvsHandle;
//NVS_Params nvsParams;
NVS_Attrs regionAttrs;

//char flashBuf0[0x2000] __attribute__ ((at(0x48000)));
//char flashBuf0[0x2000]  __attribute__ ((retain, noinit, at(0x48000)));
char flashBuf0[0x2000] __attribute__((section(".nvs"), used));
// Packet counters

unsigned int Recd = 0;
unsigned int Sent = 0;
unsigned int Bad = 0;

static uint8_t         input[MAX_LENGTH+2];
int32_t             UARTwrite_semStatus;
int_fast16_t        status = UART2_STATUS_SUCCESS;
volatile uint8_t packetRxCb;
volatile uint16_t bytesReadCount;
unsigned int Runtime = 0;
unsigned int tdmastart_timestamp = 0;
unsigned int Timer_per = 200; //periodeic timer, 20000 * 10 uS = 200 mS
unsigned int Timer_tdm = 20;  // tdma master timer, set to a low value initially so that master starts quickly
unsigned int Timer_def = 1;  // defer timer *** problem, should be possible to set .1 mS
unsigned int Timer0 = 500;
unsigned int Timer1 = 0;
extern unsigned int deviation;
extern unsigned int bitrate;
extern unsigned int rxBw;
extern int EthEna;
extern int tdelay;
int parchange;
/* Buffer which contains all Data Entries for receiving data.
 * Pragmas are needed to make sure this buffer is 4 byte aligned (requirement from the RF Core) */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_ALIGN (rxDataEntryBuffer, 4);
static uint8_t
rxDataEntryBuffer[RF_QUEUE_DATA_ENTRY_BUFFER_SIZE(NUM_DATA_ENTRIES,
                                                  MAX_LENGTH,
                                                  NUM_APPENDED_BYTES)];
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma data_alignment = 4
static uint8_t
rxDataEntryBuffer[RF_QUEUE_DATA_ENTRY_BUFFER_SIZE(NUM_DATA_ENTRIES,
                                                  MAX_LENGTH,
                                                  NUM_APPENDED_BYTES)];
#elif defined(__GNUC__)
static uint8_t
rxDataEntryBuffer[RF_QUEUE_DATA_ENTRY_BUFFER_SIZE(NUM_DATA_ENTRIES,
                                                  MAX_LENGTH,
                                                  NUM_APPENDED_BYTES)]
                                                  __attribute__((aligned(4)));
#else
#error This compiler is not supported.
#endif

/* Receive dataQueue for RF Core to fill in data */
static dataQueue_t dataQueue;
static rfc_dataEntryGeneral_t* currentDataEntry;
static uint8_t packetLength;
static uint8_t* packetDataPointer;

//static uint8_t packet[MAX_LENGTH + NUM_APPENDED_BYTES - 1]; /* The length byte is stored in a separate variable */
static uint8_t packet[MAX_LENGTH + NUM_APPENDED_BYTES]; /* The length byte is stored in a separate variable */

/***** Function definitions *****/
static void ReceivedOnRFcallback(RF_Handle h, RF_CmdHandle ch, RF_EventMask e);
static void ReceiveonUARTcallback(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status);
static void TimerCallbackFunction(void);
//static void w5500int(uint_least8_t index);

struct Settings {
    unsigned int frequency;
    unsigned char myaddr;
    unsigned char peeraddr;
    unsigned char mode;
};
char GetRssi() {
    return(RF_getRssi(rfHandle));
}

void RX_OFF(void) {
//    int status;
      RF_cancelCmd(rfHandle, rfPostHandle, 1);
}

void RX_ON(void) {
//    int status;
    rfPostHandle = RF_postCmd(rfHandle, (RF_Op*)&RF_cmdPropRx,
                                                         RF_PriorityNormal, &ReceivedOnRFcallback,
                                                         RF_EventRxEntryDone);
}

void RF_XMIT(uint8_t *message,char count) {
    int status,col;
    if (count >= 254)
        count = 254;

/*    for (i=0; i<count; i++)
    {
        packet[i] = message[i];
    } */
//    delay(1);
    RF_cmdPropTx.pktLen = count;
    RF_cmdPropTx.pPkt = message;
    col = 0;
//    while(RF_getRssi(rfHandle) >= -100) {col=1;}   // poor mans cca, hang here until rssi is below -100,
    if(col) rfcollision++;
//    delay(tdelay);
    status = RF_runCmd(rfHandle, (RF_Op*)&RF_cmdPropTx, RF_PriorityNormal, NULL, 0);
    if(status != 2) {
        xprint("what?");
    }
/*    if((packet[0] == 0x0f) && (packet[11] != 0x55)) {
        xprint(" S bad ");
        xprint_xchar(packet[11]);
        xprint("\n");
    } */
    Sent++;
    GPIO_toggle(CONFIG_GPIO_GLED);

}
void SendPacket(uint8_t  *message,char count){

       /*Cancel the ongoing command*/
       RX_OFF();
       /*Send packet*/
       RF_XMIT(message, count);
//       status = RF_runCmd(rfHandle, (RF_Op*)&RF_cmdPropTx, RF_PriorityNormal, NULL, 0);
       /* Resume RF RX */
       RX_ON();
}
void SendText(uint8_t *sndbuf,int length) {
       status = UART2_write(uart, sndbuf, length, NULL);
       if (status != UART2_STATUS_SUCCESS) {
//                     UART2_write() failed
           while (1);
       }
}
// Overrides for CMD_PROP_RADIO_DIV_SETUP
uint32_t our_overrides[] =
{
    // override_txsub1_placeholder.xml
    // The TX Power element should always be the first in the list
    TXSUB1_POWER_OVERRIDE(0x003F),
    // override_prop_common.xml
    // Tx: Set DCDC settings IPEAK=7, dither = off
    (uint32_t)0x004388D3,
    // override_prop_common_sub1g.xml
    // TX: Set FSCA divider bias to 1
    HW32_ARRAY_OVERRIDE(0x405C,0x0001),
    // TX: Set FSCA divider bias to 1
    (uint32_t)0x08141131,
    // override_tc782.xml
    // Tx: Configure PA ramp time, PACTL2.RC=0x1 (in ADI0, set PACTL2[4:3]=0x1)
    ADI_2HALFREG_OVERRIDE(0,16,0x8,0x8,17,0x1,0x1),
    // Rx: Set AGC reference level to 0x2E
    HW_REG_OVERRIDE(0x609C,0x002E),
    // Rx: Set RSSI offset to adjust reported RSSI by -4 dB at 779-930 MHz
    (uint32_t)0x000488A3,
    // Set LNA IB boost to 2
    ADI_HALFREG_OVERRIDE(0,5,0xF,0x2),
    // Rx: Set anti-aliasing filter bandwidth to 0x0 (in ADI0, set IFAMPCTL3[7:4]=0x0)
    ADI_HALFREG_OVERRIDE(0,61,0xF,0x0),
    // Tx: Configure PA ramping, set wait time before turning off (0x1A ticks of 16/24 us = 17.3 us).
    HW_REG_OVERRIDE(0x6028,0x001A),
    // TX: set intFreq = 0
    (uint32_t)0x00000343,
    (uint32_t)0xFFFFFFFF
};

TaskHandle_t Radioprog;
int timestamp;

void mainThread(void *arg0)
{
    TaskStatus_t radtask;
    uint8_t *op;
    vTaskGetInfo(NULL, &radtask, NULL, eInvalid);
    op = radtask.pcTaskName;
    memcpy(op,"Radio\0",6);
    Radioprog = xTaskGetCurrentTaskHandle();

//    char idle; // time left indicator
    uint8_t buff[Nsize];
//    char chk = 0;
    packetRxCb = NO_PACKET;

    SPI_init();
    NVS_init();
#ifdef MEMLOG
    loginit();
    dolog("Amprnet Radio logger  \r\n", 24, 0);
#endif
//    NVS_Params_init(&nvsParams);

    Timer_Params_init(&Timparams);
    Timparams.periodUnits = Timer_PERIOD_HZ;
    Timparams.period = 1000;
    Timparams.timerMode  = Timer_CONTINUOUS_CALLBACK;
    Timparams.timerCallback = (void *)TimerCallbackFunction;
    //
    Timhandle = Timer_open(CONFIG_TIMER_0, &Timparams);
    if (Timhandle == NULL) {
        // Timer_open() failed
        while (1);
    }
    status = Timer_start(Timhandle);
    if (status == Timer_STATUS_ERROR) {
        //Timer_start() failed
        while (1);
    }

    RF_Params rfParams;
    RF_Params_init(&rfParams);

    if(RFQueue_defineQueue(&dataQueue,
                                rxDataEntryBuffer,
                                sizeof(rxDataEntryBuffer),
                                NUM_DATA_ENTRIES,
                                MAX_LENGTH + NUM_APPENDED_BYTES))
    {
        /* Failed to allocate space for all data entries */
        while(1);
    }
//    GPIO_setConfig(1,GPIO_CFG_IN_PU);
//    GPIO_setConfig(2,GPIO_CFG_IN_PU);

    GPIO_setConfig(CONFIG_GPIO_RLED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_write(CONFIG_GPIO_RLED, CONFIG_GPIO_LED_OFF);

    GPIO_setConfig(CONFIG_GPIO_GLED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_write(CONFIG_GPIO_GLED, CONFIG_GPIO_LED_OFF);
#ifdef N536RADIO
    IOCPortConfigureSet(RXEN, IOC_PORT_RFC_GPO0,IOC_IOMODE_NORMAL);
    IOCPortConfigureSet(PAEN, IOC_PORT_RFC_GPO1,IOC_IOMODE_INV);
    IOCPortConfigureSet(TXEN, IOC_PORT_RFC_GPO3,IOC_IOMODE_NORMAL);
    IOCPortConfigureSet(LNAEN, IOC_PORT_RFC_GPO0,IOC_IOMODE_INV);

#else
    IOCPortConfigureSet(LNA_HIGH, IOC_PORT_RFC_GPO0,IOC_IOMODE_NORMAL);
    IOCPortConfigureSet(PA_HIGH, IOC_PORT_RFC_GPO1,IOC_IOMODE_NORMAL);
    IOCPortConfigureSet(TX_HIGH, IOC_PORT_RFC_GPO3,IOC_IOMODE_NORMAL);
#endif
    /*  toggle led pins to show module is alive */
        GPIO_toggle(CONFIG_GPIO_GLED);
        delay(1000);
        GPIO_toggle(CONFIG_GPIO_RLED);
        delay(1000);
        GPIO_toggle(CONFIG_GPIO_GLED);
        delay(1000);
        GPIO_toggle(CONFIG_GPIO_RLED);
        delay(1000);

// set up the interrupt pin
//    GPIO_setCallback(EINT, w5500int);
//    GPIO_enableInt(EINT);
    /*Modifies settings to be able to do RX*/
    /* Set the Data Entity queue for received data */
    RF_cmdPropRx.pQueue = &dataQueue;
    /* Discard ignored packets from Rx queue */
    RF_cmdPropRx.rxConf.bAutoFlushIgnored = 1;
    /* Hook rssi to end of packet GW 0.3 */
    RF_cmdPropRx.rxConf.bAppendRssi = 1;
    /* Discard packets with CRC error from Rx queue */
    RF_cmdPropRx.rxConf.bAutoFlushCrcErr = 1;
    /* Implement packet length filtering to avoid PROP_ERROR_RXBUF */
    RF_cmdPropRx.maxPktLen = MAX_LENGTH;
    RF_cmdPropRx.pktConf.bRepeatOk = 1;
    RF_cmdPropRx.pktConf.bRepeatNok = 1;
    /* transmit properties */
    RF_cmdPropTx.pPkt = packet;
    RF_cmdPropTx.startTrigger.triggerType = TRIG_NOW;
#ifdef SYN4
    RF_cmdPropTx.syncWord = 0xdf5f55df;     //4fsk
    RF_cmdPropRx.syncWord = 0xdf5f55df;     //4fsk
#endif
    /* Set the max amount of bytes to read via UART */
//    size_t bytesToRead = MAX_LENGTH;

    bytesReadCount = 0;

    /* Initialize UART with callback read mode */
    UART2_Params_init(&uartParams);
    uartParams.baudRate = 115200;
    uartParams.readMode = UART2_Mode_CALLBACK;
    uartParams.readCallback = ReceiveonUARTcallback;
    uartParams.readReturnMode = UART2_ReadReturnMode_PARTIAL;

    init_uart_1();
    /* Access UART */
    uart = UART2_open(CONFIG_UART2_0, &uartParams);

    /* Print to the terminal that the program has started */
    const char        startMsg[] = "\r\nRF-UART bridge started:\r\n";
    UART2_write(uart, startMsg, sizeof(startMsg), NULL);
    start_terminal();
    // Open NVS driver instance
//    nvsHandle = NVS_open(CONFIG_INTERNAL, &nvsParams);
    get_NVS(buff);
    NVS_getAttrs(nvsHandle, &regionAttrs);
    printMAC();
    myaddr = m6;    // set myaddr to last byte of MAC if not saved
    xprint("\n");
    NVS_close(nvsHandle);
#if defined LCD
    LcdEna = LCD_Begin();
    if(LcdEna == 0) {
        xprint("LCD ON");
        delay(1);
        LCD_Print(version);
        delay(1);
    } else {
        xprint("NO LCD");
    }
#endif
    xprint("\n");
    xprint("$ ");

    /* */
#ifdef HAM23CMRADIO
    RF_cmdPropRadioDivSetup.loDivider = 0x04;
    RF_cmdPropRadioDivSetup.centerFreq = freq;
//    RF_cmdPropRadioDivSetup.intFreq = 3481;
    RF_cmdPropRadioDivSetup.txPower = 0xa73f;   //GW power
//
#ifdef FSK4
    RF_cmdPropRadioDivSetup.formatConf.fecMode = 9; // GW 02-dec-25 enable 4fsk
    RF_cmdPropRadioDivSetup.modulation.deviation = 200; // 1/3 mod at 2FSK
#endif

//    RF_cmdPropRadioDivSetup.symbolRate.rateWord = 0xe0000;   //GW speed 1400
//    RF_cmdPropRadioDivSetup.symbolRate.rateWord = 0xf0000;   //GW speed 1500
//    RF_cmdPropRadioDivSetup.modulation.deviation = 0x232; // GW deviation 750 *0.75
//    RF_cmdPropRadioDivSetup.modulation.deviation = 0x2ee; // GW deviation 750 *0.75
//    RF_cmdPropRadioDivSetup.modulation.deviation = 350; // GW deviation 700
//    RF_cmdPropRadioDivSetup.rxBw = 100; // 2185 khz
//    RF_cmdPropRadioDivSetup.rxBw = 101; // 2486 khz
//    RF_cmdPropRadioDivSetup.rxBw = 95; // 3134 khz
//    RF_cmdPropRadioDivSetup.symbolRate.rateWord = 0xc0000;   //GW speed 1200
//    RF_cmdPropRadioDivSetup.modulation.deviation = 500; // GW 18-dec-2024
    RF_cmdPropRadioDivSetup.rxBw = rxBw; // 3134 khz
    RF_cmdPropRadioDivSetup.modulation.deviationStepSz = 1; // GW deviation step 1000 hz
    RF_cmdPropRadioDivSetup.symbolRate.rateWord = bitrate;   //GW speed 1500
    RF_cmdPropRadioDivSetup.modulation.deviation = deviation; // GW 18-dec-2024
    RF_cmdPropRadioDivSetup.pRegOverride = our_overrides;
#endif
    /* Request access to the radio */
    rfHandle = RF_open(&rfObject, &RF_prop, (RF_RadioSetup*)&RF_cmdPropRadioDivSetup, &rfParams);


    RF_cmdPropRadioDivSetup.config.biasMode = 0x1;
    RF_cmdFs.frequency = freq;


    /* Set the frequency */
    RF_postCmd(rfHandle, (RF_Op*)&RF_cmdFs, RF_PriorityNormal, NULL, 0);

    rfPostHandle = RF_postCmd(rfHandle, (RF_Op*)&RF_cmdPropRx,
                                                           RF_PriorityNormal, &ReceivedOnRFcallback,
                                                           RF_EventRxEntryDone);

    size_t bytesToRead = MAX_LENGTH-2; //GW 241020
    UART2_read(uart, &input, bytesToRead, NULL);
    while(1)
    {
        amprEntry_t amprEntry = ampr_dequeueRadio(100);
        loopctr++;

        /* Check if anything has been received via RF*/
        if(amprEntry.type == AMPR_QUEUE_RX_DATA)      // if !=0 we have a rf packet
        {
            /* Get current unhandled data entry */
            currentDataEntry = RFQueue_getDataEntry(); //loads data from entry

            /* Handle the packet data, located at &currentDataEntry->data:
             * - Length is the first byte with the current configuration
             * - Data starts from the second byte */
            packetLength      = *(uint8_t*)(&currentDataEntry->data); //gets the packet length (send over with packet)
            packetDataPointer = (uint8_t*)(&currentDataEntry->data + 1); //data starts from 2nd byte

            /* Copy the payload + the status byte to the packet variable */
            memcpy(packet, packetDataPointer, (packetLength + 1));
            /* Move read entry pointer to next entry */
            RFQueue_nextEntry();

            rssi =  packet[packetLength]; // get rssi

            //          go select who should process packet
            whatpacket(packet, (packetLength));

            // If it was a TDMA packet and it is time for our slot now, send it
            if(myslot == 1)
            {
                // Send Ethernet packet if any
                if(!dequeue_eth()) { // Send Ethernet packet if any in queue
                    if((uartlen() != 0)) {
                          current_defer = 80 + uartlen();
//                          GPIO_write(sigpin2,1);
                          RX_OFF();
                          dequeue_uart();
                          RX_ON();
//                          GPIO_write(sigpin2,0);
                      }
                }
                // Send TDMA (whether or not we sent an Ethernet packet)
                 RX_OFF();
                 send_tdma_packet();
                 RX_ON();

                 if(role == MASTER)
                 {
                     // Reset the TDMA cycle timer.
                     Timer_tdm = TDMAPERIOD;
                     tdmastart_timestamp = Runtime;
                 }
                 myslot = 0;
                 GPIO_write(sigpin,0);
            }
        } // If radio RX received

        /* myslot controls transmission and is detected by a slave seeing a tdma packet with the mac address before him in the tlst

        a master knows the address of the last slave, since he detected that while traversing the ctab it was the last entry he added.

        so, the cycle is 1:st slave, 2:nd slave ....last slave, master , 1:st slave..... There are no defers within the cycle, unless the master

        sends an invite. In that case he uses the defer timer, to set a limit to the wait for a connect. If a connect arrives at the master,

        he will enter the node in ctab, and he will cancel the timer_def, not to wait unnessecarily. Slaves advance their comparison

        on all packets except invite. After detecting a connect packet, the master will set his myslot variable, and will

        send a synch as his normal tdma packet, with the new list.  To get the whole thing going the Timer_tdm is used to force a

        transmssion where there is no previous cycle going. This is only "cold start" or "no slaves" detection */

        // Only the master uses the TX_SLOT event which is triggered by a timer.
        // The event is used to start a new TDMA cycle if the current one stalled
        // due to a slave not sending a TDMA message in its slot.
        // However, if a new cycle has already been started this event should be ignored.
        // Currently this can happen if event processing is delayed due to printing to the console.
        else if(role == MASTER && amprEntry.type == AMPR_QUEUE_TX_SLOT && Timer_tdm == 0) // Check that a new cycle is not already started (Timer_tdm already re-armed)
        {
            // Send Ethernet packet if any
            if(!dequeue_eth()) { // Send Ethernet packet if any in queue
                if((uartlen() != 0)) {
                      current_defer = 80 + uartlen();
//                      GPIO_write(sigpin2,1);
                      RX_OFF();
                      dequeue_uart();
                      RX_ON();
//                      GPIO_write(sigpin2,0);
                  }
            }
            // Send TDMA (whether or not we sent an Ethernet packet)
             RX_OFF();
             send_tdma_packet();
             RX_ON();

             if(role == MASTER)
             {
                 // Reset the TDMA cycle timer.
                 Timer_tdm = TDMAPERIOD;
                 tdmastart_timestamp = Runtime;
             }
             myslot = 0;
        } // TX_SLOT


#ifdef  HAM23CMRADIO
     if(parchange != 0) {
          RX_OFF();       // RX_OFF executes RF_cancelCmd(rfHandle, rfPostHandle, 1);
            RF_close(rfHandle);
            RF_cmdPropRadioDivSetup.rxBw = rxBw; // 3134 khz
            RF_cmdPropRadioDivSetup.modulation.deviationStepSz = 1; // GW deviation step 1000 hz
            RF_cmdPropRadioDivSetup.symbolRate.rateWord = bitrate;   //GW speed 1500
            RF_cmdPropRadioDivSetup.modulation.deviation = deviation; // GW 18-dec-2024
            rfHandle = RF_open(&rfObject, &RF_prop, (RF_RadioSetup*)&RF_cmdPropRadioDivSetup, &rfParams);
            rfPostHandle = RF_postCmd(rfHandle, (RF_Op*)&RF_cmdPropRx,
                                                                 RF_PriorityNormal, &ReceivedOnRFcallback,
                                                                 RF_EventRxEntryDone);
            RX_ON(); // RX_ON executes    rfPostHandle = RF_postCmd(rfHandle, (RF_Op*)&RF_cmdPropRx,RF_PriorityNormal, &ReceivedOnRFcallback,RF_EventRxEntryDone);
            RF_postCmd(rfHandle, (RF_Op*)&RF_cmdFs, RF_PriorityNormal, NULL, 0);
            parchange =0;   //only execute once
        }
        if (freq != freqold) {          // set new frequency
            freqold = freq;
            RF_cmdFs.frequency = freq;
            RF_postCmd(rfHandle, (RF_Op*)&RF_cmdFs, RF_PriorityNormal, NULL, 0);
        }
#endif

        checkcommand();

        // Reboot request from shell command
        if(rebootRequest)
        {
            SysCtrlSystemReset();
        }
    }
}

/* Callback function called when data is received via RF
 * Function copies the data in a variable, packet, and sets packetRxCb */
void ReceivedOnRFcallback(RF_Handle h, RF_CmdHandle ch, RF_EventMask e)
{
    if (e & RF_EventRxEntryDone)
    {
        timestamp = Runtime;
        GPIO_toggle(CONFIG_GPIO_RLED);
        ampr_queueRadioRXFromISR();
        Recd++;
    }
}

/* Callback function called when data is received via UART */
void ReceiveonUARTcallback(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status)
{
    if (status != UART2_STATUS_SUCCESS)
    {
        /* RX error occured in UART2_read() */
        while (1) {}
    }
      bytesReadCount = count; //241025
      queue_uart(buffer,count); // 241024
      status = UART2_read(uart, &input, 253, NULL); //241026
}

void TimerCallbackFunction(void) {
    Runtime++;
    if(Timer0) Timer0--;    // decrement timer if counting
    if(Timer1) Timer1--;
    if(Timer_per) Timer_per--;
    if(Timer_tdm) {
        Timer_tdm--;
        if(Timer_tdm == 0)
            ampr_queueRadioTX(); // Issue a TX slot event when the timer reaches 0 to indicate that it is time to send
    }
}
/* unsigned int eints = 0;
uint8_t pktbuf1[1514];
void w5500int(uint_least8_t index) {
    int reclen;
    reclen = w5500readFrame(pktbuf1, sizeof(pktbuf1));
    queue_eth(pktbuf1,reclen, eints & 0xff);
    setSIR(1);  // only socket 0
    setSn_IR(4);
    eints++;
} */
