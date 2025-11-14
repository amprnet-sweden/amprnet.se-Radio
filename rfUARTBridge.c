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

/* Board Header files */
#include "ti_drivers_config.h"

/* Application Header files */
#include "RFQueue.h"
#include "Ampr-radio.h"
#include <ti_radio_config.h>

/* ======== uart2 ======== */
#include <ti/drivers/UART2.h>
#include <ti/drivers/Temperature.h>
#include <ti/devices/cc13x1_cc26x1/driverlib/aon_batmon.h>
#include <ti/devices/cc13x1_cc26x1/driverlib/ioc.h>

/***** Defines *****/

/* Packet RX Configuration */
#define DATA_ENTRY_HEADER_SIZE 8  /* Constant header size of a Generic Data Entry */
#define MAX_LENGTH             255 /* Max length byte the radio will accept */
#define NUM_DATA_ENTRIES       8  /* NOTE: Only two data entries supported at the moment */
#define NUM_APPENDED_BYTES     2  /* The Data Entries data field will contain:
                                   * 1 Header byte (RF_cmdPropRx.rxConf.bIncludeHdr = 0x1)
                                   * Max 30 payload bytes
                                   * 1 status byte (RF_cmdPropRx.rxConf.bAppendStatus = 0x1) */
#define NO_PACKET              0
#define PACKET_RECEIVED        1
#define Nsize 80                // size of command buffer and get parameter buffer


/*******Global variable declarations*********/
static RF_Object rfObject;
static RF_Handle rfHandle;
RF_CmdHandle rfPostHandle;

UART2_Handle uart;
UART2_Params uartParams;


// nvram
NVS_Handle nvsHandle;
NVS_Attrs regionAttrs;

char flashBuf0[0x2000] __attribute__((section(".nvs"), used));

// Packet counters

unsigned int Recd = 0;
unsigned int Sent = 0;
unsigned int Bad = 0;

static char         input[MAX_LENGTH+2];
int32_t             UARTwrite_semStatus;
int_fast16_t        status = UART2_STATUS_SUCCESS;
volatile uint8_t packetRxCb;
volatile size_t bytesReadCount;
extern int EthEna;

unsigned int Timer0 = 500;
unsigned int Timer1 = 0;
unsigned int Runtime;



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

static uint8_t packet[MAX_LENGTH + NUM_APPENDED_BYTES]; /* The length byte is stored in a separate variable */

/***** Function definitions *****/
static void ReceivedOnRFcallback(RF_Handle h, RF_CmdHandle ch, RF_EventMask e);
static void ReceiveonUARTcallback(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status);
static void TimerCallbackFunction(void);


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
//    xprint("E");
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
char eseg;
void check_seg(char * buffer, int len){
    uint8_t seg,expseg,fin;
    seg = buffer[0] & 0x7;
    fin = buffer[0] & 0x8;

    if (seg != eseg) {
        xprint("SO ");
        xprint_char(seg);
        xprint(" E ");
        xprint_char(eseg);
        xprint("\n");
    }
    if(seg == 7) {
        eseg = 0;
    } else {
      if(fin) {
        eseg = 7;
      } else {
        eseg = seg + 1;
      }
    }
//   xprint("SG ");
//   xprint_xchar(seg);
//   if(fin) xprint(" F");
//   xprint("\n");

}

void whatpacket(uint8_t * buffer, char length) {
    uint8_t pktype;
//    Timer1 = 20;        // mark recv in progress, do not interfere
    pktype = buffer[0] & 0xf0;
//    GPIO_write(sigpin2,1);
    switch(pktype) {
    case PETH:
        if(EthEna) {
/* copy the buffer to ethernet task
 * signal the ethernet task
 */
//            send_log_packet("RR\n");
//            check_seg(buffer,length);
            send_ether(buffer,length);
        }
        break;
    case PTXT:  // this is a "uart" packet, send it out
        SendText(&buffer[1], length - 1);
        URpkts++;
        URbytes += length -1;
        break;
    case PPTP:
        xprint("PTP pktype\n");
        break;
    case PXXX:
        xprint("XXX pktype\n");
        break;
    default:
        xprint("Bad pktype : ");
        xprint_char(pktype);
        xprint("\n");
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
int snapshot;

void *mainThread(void *arg0)
{
    TaskStatus_t radtask;
    uint8_t *op;
    vTaskGetInfo(NULL, &radtask, NULL, eInvalid);
    op = radtask.pcTaskName;
    memcpy(op,"Radio\0",6);
    Radioprog = xTaskGetCurrentTaskHandle();

    uint8_t buff[Nsize];
    packetRxCb = NO_PACKET;

    SPI_init();
    NVS_init();
//    NVS_Params_init(&nvsParams);

    Timer_Params_init(&Timparams);
    Timparams.periodUnits = Timer_PERIOD_HZ;
//    Timparams.period = 1000;
    Timparams.period = 100000;
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

    GPIO_setConfig(CONFIG_GPIO_RLED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_write(CONFIG_GPIO_RLED, CONFIG_GPIO_LED_OFF);

    GPIO_setConfig(CONFIG_GPIO_GLED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_write(CONFIG_GPIO_GLED, CONFIG_GPIO_LED_OFF);

// set up LAN PA and TX indication pins

    IOCPortConfigureSet(LNA_HIGH, IOC_PORT_RFC_GPO0,IOC_IOMODE_NORMAL);
    IOCPortConfigureSet(PA_HIGH, IOC_PORT_RFC_GPO1,IOC_IOMODE_NORMAL);
    IOCPortConfigureSet(TX_HIGH, IOC_PORT_RFC_GPO3,IOC_IOMODE_NORMAL);


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

    /* Set TX properties also */
    RF_cmdPropTx.pPkt = packet;
    RF_cmdPropTx.startTrigger.triggerType = TRIG_NOW;

    /* Set the max amount of bytes to read via UART */
//    size_t bytesToRead = MAX_LENGTH;

    bytesReadCount = 0;

    /* Initialize UART with callback read mode */
    UART2_Params_init(&uartParams);
    uartParams.baudRate = 115200;
    uartParams.readMode = UART2_Mode_CALLBACK;
    uartParams.readCallback = ReceiveonUARTcallback;
    uartParams.readReturnMode = UART2_ReadReturnMode_PARTIAL;

    /* Access UART */
    uart = UART2_open(CONFIG_UART2_0, &uartParams);

    /* Print to the terminal that the program has started */
    const char        startMsg[] = "\r\nRF-UART bridge started:\r\n";
    UART2_write(uart, startMsg, sizeof(startMsg), NULL);

// Open NVS driver instance
//    nvsHandle = NVS_open(CONFIG_INTERNAL, &nvsParams);
    get_NVS(buff);
    NVS_getAttrs(nvsHandle, &regionAttrs);
    xprint("EthEna was = ");
    xprint_char(EthEna);
    xprint("\n");
//    if (EthEna == 1) {
//      char stat = init_ether();
//    }
    printMAC();
    myaddr = m6;    // set myaddr to last byte of MAC if not saved
    xprint("\n");
    NVS_close(nvsHandle);
#if defined LCD
    LcdEna = LCD_Begin();
    if(LcdEna == 0) {
        xprint("LCD ON");
    } else {
        xprint("NO LCD");
    }
#endif
    xprint("\n");
    xprint("$ ");

    /* */
#ifdef HAM23CMRADIO
    RF_cmdPropRadioDivSetup.loDivider = 0x04; // set lo divider
    RF_cmdPropRadioDivSetup.centerFreq = freq;
    RF_cmdPropRadioDivSetup.txPower = 0xa73f;   //GW power
//
    RF_cmdPropRadioDivSetup.symbolRate.rateWord = 0xc0000;   //GW speed 1200
    RF_cmdPropRadioDivSetup.pRegOverride = our_overrides;
#endif
    /* Request access to the radio */
    rfHandle = RF_open(&rfObject, &RF_prop, (RF_RadioSetup*)&RF_cmdPropRadioDivSetup, &rfParams);
    //    RF_cmdPropRadioDivSetup.centerFreq = 0x04ec;
    //    RF_cmdPropRadioDivSetup.intFreq = 0x0d99;
        RF_cmdPropRadioDivSetup.config.biasMode = 0x1; // should this move 4 lines up?? GW
    //    RF_cmdPropRadioDivSetup.loDivider = 0x04; */
    //    RF_cmdFs.frequency = 0x4ec;
        RF_cmdFs.frequency = freq;


    /* Set the frequency */
    RF_postCmd(rfHandle, (RF_Op*)&RF_cmdFs, RF_PriorityNormal, NULL, 0);

    rfPostHandle = RF_postCmd(rfHandle, (RF_Op*)&RF_cmdPropRx,
                                                           RF_PriorityNormal, &ReceivedOnRFcallback,
                                                           RF_EventRxEntryDone);
    size_t bytesToRead = MAX_LENGTH-2; //GW 241020
    UART2_read(uart, &input, bytesToRead, NULL);
    while(1)  {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
//        GPIO_write(sigpin2,0);
        if(debug & 4) {
          snapshot = Runtime - timestamp;
          if(snapshot >= 40) {
            xprint_int(snapshot *10);
            xprint(" uS\n");
          }
        }
        loopctr++;

        /* Check if anything has been received via RF*/
//        if(packetRxCb)
//        if (1 == 1)
        do
        {
            currentDataEntry = RFQueue_getDataEntry(); //loads data from entry
//            xprint_int(currentDataEntry->status);
//            xprint("\n");
//            while(currentDataEntry->status == 3) {
            /* Handle the packet data, located at &currentDataEntry->data:
             * - Length is the first byte with the current configuration
             * - Data starts from the second byte */
            packetLength      = *(uint8_t*)(&currentDataEntry->data); //gets the packet length (send over with packet)
            packetDataPointer = (uint8_t*)(&currentDataEntry->data + 1); //data starts from 2nd byte
            /* Copy the payload + the status byte to the packet variable */
            memcpy(packet, packetDataPointer, (packetLength + 1));

            /* Move read entry pointer to next entry */
            RFQueue_nextEntry();
            memcpy(input, packet, (packetLength));
            rssi =  packet[packetLength]; // get rssi of last packet
            packetRxCb = NO_PACKET;
            //          go select who should process packet
            whatpacket(input, (packetLength));
//            }
        } while (currentDataEntry->status == 3);
#ifdef  HAM23CMRADIO
        if (freq != freqold) {          // set new frequency
            freqold = freq;
            RF_cmdFs.frequency = freq;
            RF_postCmd(rfHandle, (RF_Op*)&RF_cmdFs, RF_PriorityNormal, NULL, 0);
        }
#endif
#ifdef ETHERNET
//        if (EthEna == 1) {
//              check_ethernet();
//        }
#endif
              /* Check if anything has been received via UART*/
              if (bytesReadCount != 0)
              {
                  bytesReadCount = 0;
              }


    }
}

/* Callback function called when data is received via RF
 * Function copies the data in a variable, packet, and sets packetRxCb */
void ReceivedOnRFcallback(RF_Handle h, RF_CmdHandle ch, RF_EventMask e)
{
//    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t xHigherPriorityTaskWoken = pdTRUE;
    if (e & RF_EventRxEntryDone)
    {
        timestamp = Runtime;
        GPIO_toggle(CONFIG_GPIO_RLED);
//        GPIO_write(sigpin2,1);
        /* Get current unhandled data entry */
//        currentDataEntry = RFQueue_getDataEntry(); //loads data from entry

        /* Handle the packet data, located at &currentDataEntry->data:
         * - Length is the first byte with the current configuration
         * - Data starts from the second byte */
//        packetLength      = *(uint8_t*)(&currentDataEntry->data); //gets the packet length (send over with packet)
//        packetDataPointer = (uint8_t*)(&currentDataEntry->data + 1); //data starts from 2nd byte

        /* Copy the payload + the status byte to the packet variable */
//        memcpy(packet, packetDataPointer, (packetLength + 1));

        /* Move read entry pointer to next entry */
//        RFQueue_nextEntry();

        packetRxCb = PACKET_RECEIVED;
//        GPIO_toggle(sigpin2);
    //    vTaskNotifyGiveFromISR(thisprog, &xHigherPriorityTaskWoken);
        xTaskNotifyFromISR(Radioprog,1,eSetBits,&xHigherPriorityTaskWoken );
        xHigherPriorityTaskWoken = pdTRUE;
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
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
//      bytesReadCount = count; //241025
      queue_uart(buffer,count); // 241024
      status = UART2_read(uart, &input, 253, NULL); //241026
}

void TimerCallbackFunction(void) {
    Runtime++;
    if(Timer0 != 0) {
        Timer0--;    // decrement timer if counting
    }
    if(Timer1 != 0) {
        Timer1--;
    }
}

