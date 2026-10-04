#include "ethBuf.h"
#include "Ampr-radio.h"
#include <FreeRTOS.h>
#include <queue.h>

//    int eiidx = 0;
//    int eoidx = 0;

// Statically allocated buffers
uint8_t ebufs[EBCOUNT][EBSIZE];
//    int ebcount[EBCOUNT];
//    uint8_t ebnumber[EBCOUNT];

QueueHandle_t bufferPool;

void ethBuf_init()
{
    // Add all buffers to the free buffer pool
    bufferPool = xQueueCreate(EBCOUNT, sizeof(ethBufHandle_t));
    ethBufHandle_t buf = { .buffer = NULL, .bytesUsed = 0, .packetNumber = 0 };
    for(int i = 0; i < EBCOUNT; i++) {
        buf.buffer = (uint8_t*)ebufs[i];
        BaseType_t result = xQueueSend(bufferPool, &buf, 0);
        if(result != pdPASS) {
            xprint("Failed to init Ethernet buffer pool\n");
            break;
        }
    }
}

void ethBuf_get(ethBufHandle_t* buf) {
    BaseType_t result = xQueueReceive(bufferPool, buf, 0);
    if(result != pdPASS) {
        buf->buffer = NULL;
        buf->bytesUsed = 0;
    } else {
        if(ampr_poolSize() < poolmin)
        	poolmin = ampr_poolSize();
    }
}

void ethBuf_free(ethBufHandle_t* buf) {
    if(buf && buf->buffer != NULL) {
        buf->bytesUsed = 0;
        xQueueSend(bufferPool, buf, 0);
        buf->buffer = NULL;
        buf->bytesUsed = 0;
        buf->packetNumber = 0;
    }
}
uint16_t ampr_poolSize() {
//    return (uint16_t)uxQueueMessagesWaiting(bufferPool);
    return uxQueueMessagesWaiting(bufferPool);
}
