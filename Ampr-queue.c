#include "Ampr-queue.h"

/* RTOS header files */
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

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
uint16_t droppedEthPackets = 0;
uint16_t droppedRadioPackets = 0;

void ampr_initQueue() {
    amprQueue = xQueueCreate(EBCOUNT+RADIOCOUNT, sizeof(amprEntry_t));
}

void ampr_queueEth(ethBufHandle_t* bufferHandle) {
    amprEntry_t entry;
    entry.type = AMPR_QUEUE_ETH;
    entry.ethHandle = *bufferHandle;
    if(!xQueueSend(amprQueue, &entry, 0)) {
        ethBuf_free(bufferHandle);
        droppedEthPackets++;
    }
}

amprEntry_t ampr_dequeue(uint32_t timeout_ms) {
    amprEntry_t entry = {
                         .type = AMPR_QUEUE_NONE,
                         .ethHandle = { .buffer = NULL, .bytesUsed = 0, .packetNumber = 0 }
    };
    xQueueReceive(amprQueue, &entry, pdMS_TO_TICKS(timeout_ms));
    return entry; // Packet type NONE if queue was empty
}

void ampr_queueRadioFromISR()
{
    amprEntry_t entry;
    entry.type = AMPR_QUEUE_RADIO;
    if(!xQueueSendFromISR(amprQueue, &entry, 0)) {
        droppedRadioPackets++; // Note: The packet is not actually dropped from the RFQueue
    }
}
