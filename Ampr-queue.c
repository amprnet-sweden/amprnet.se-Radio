#include "Ampr-queue.h"

/* RTOS header files */
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include "Ampr-radio.h"

// Number of radio RX packages, defined in NUM_DATA_ENTRIES in rfUartBridge.c
#define RADIOCOUNT 16

// Queue of incoming events to the Ampr-radio logic:
// - Received packets over radio
// - Received Ethernet frames from the Ethernet interface to be set over radio
// - Ethernet packets containing e.g. icmp_replies to be sent over radio
// Note: Ethernet packets contain a handle to an Ethernet buffer.
// This handle must be either queued (in this queue or another)
// or freed using ethBuf_free()
QueueHandle_t amprQueue;
QueueHandle_t ethQueue;
uint16_t droppedEthPackets = 0;
uint16_t droppedRadioPackets = 0;
uint16_t droppedRadioSlots = 0;

void ampr_initQueue() {
    amprQueue = xQueueCreate(RADIOCOUNT, sizeof(amprEntry_t));
    ethQueue = xQueueCreate(EBCOUNT, sizeof(ethBufHandle_t));
}

void ampr_queueEth(ethBufHandle_t* bufferHandle) {
    if(!xQueueSend(ethQueue, bufferHandle, 0)) {
        ethBuf_free(bufferHandle);
        droppedEthPackets++;
        xprint("dropped\n");
    } else {
//    xprint("queued\n");
    }
}

amprEntry_t ampr_dequeueRadio(uint32_t timeout_ms) {
    amprEntry_t entry = {
                         .type = AMPR_QUEUE_NONE,
    };
    xQueueReceive(amprQueue, &entry, pdMS_TO_TICKS(timeout_ms));
    return entry; // Packet type NONE if queue was empty
}

ethBufHandle_t ampr_dequeueEth() {
    ethBufHandle_t buffer = {
                             .buffer = NULL,
                             .bytesUsed = 0,
                             .packetNumber = 0
    };
    xQueueReceive(ethQueue, &buffer, 0);
//    xprint("dequeued\n");
    return buffer;
}

uint16_t ampr_ethQueueSize() {
    return (uint16_t)uxQueueMessagesWaiting(ethQueue);
}

bool ampr_ethQueueEmpty() {
    return ampr_ethQueueSize() == 0;
}

void ampr_queueRadioRXFromISR()
{
    amprEntry_t entry;
    entry.type = AMPR_QUEUE_RX_DATA;
    if(!xQueueSendFromISR(amprQueue, &entry, 0)) {
        droppedRadioPackets++; // Note: The packet is not actually dropped from the RFQueue
    }
}

void ampr_queueRadioTXFromISR()
{
    amprEntry_t entry;
    entry.type = AMPR_QUEUE_TX_SLOT;
    if(!xQueueSend(amprQueue, &entry, 0)) {
        droppedRadioSlots++;
    }
}
