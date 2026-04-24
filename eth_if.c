#include "eth_if.h"
#include "ethBuf.h"

#include "Ampr-radio.h"
#include "Ampr-queue.h"
#include "w5500.h"

/* RTOS header files */
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
int ebufsused;

void ampr_queue(ethBufHandle_t* bufferHandle);
extern int EthEna;

// Queue of buffers containing Ethernet frames to send to the Ethernet chip
QueueHandle_t sendQueue;

uint8_t extrabuf[EBSIZE];

// TODO temporary implementation until buffer handles are used in ethproc
void ethIf_send(uint8_t* buffer, uint16_t size)
{
    ethBufHandle_t handle;
    ethBuf_get(&handle);
    if(handle.buffer == NULL) {
        return; // No free buffer, drop packet
    }
    ebufsused++;
    memcpy(handle.buffer, buffer, size);
    handle.bytesUsed = size;
    // Queue packet or drop it if the send queue is full
    if(!xQueueSend(sendQueue, &handle, 0)) {
        ethBuf_free(&handle);
        ebufsused--;
    }
}

void vEthIf_task(void* pvParameters)
{
    const TickType_t xTicksRecvPeriod = pdMS_TO_TICKS(1);

    bool deviceInitialized = false;
    uint8_t my_hwaddr[6];
    getMAC(my_hwaddr);

    sendQueue = xQueueCreate(EBCOUNT/2, sizeof(ethBufHandle_t));

    for(;;)
    {
        if(!deviceInitialized) {
            deviceInitialized = w5500begin(my_hwaddr);
            if(deviceInitialized)  {
                xprint("Ether initiated\n");
                EthEna = 1;
            } else {
                // No Ethernet chip, sleep a while then try again
                EthEna = 0;
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
        }

        ethBufHandle_t buffer = {.buffer = NULL, .bytesUsed = 0};
        BaseType_t xStatus = xQueueReceive(sendQueue, &buffer, xTicksRecvPeriod);
        if( xStatus == pdPASS )
        {
            // Send data to Ethernet chip
            w5500sendFrame(buffer.buffer, buffer.bytesUsed);
            ethBuf_free(&buffer);
        }

        // Receive data from the Ethernet chip

        // First get a buffer to put the data in
        ethBuf_get(&buffer);
        if(buffer.buffer == NULL) {// No available buffer
            continue;
        }
        int reclen = w5500readFrame(buffer.buffer, EBSIZE);
        buffer.bytesUsed = reclen;
        if(reclen > 0) {
//            GPIO_write(sigpin4,1);
            // Don't put the packet in the radio queue if it is for me
            if (memcmp (buffer.buffer, my_hwaddr,6) == 0) { // if this packet was for me
                proc_eth(buffer.buffer, buffer.bytesUsed, PORT_ETH);
                ethBuf_free(&buffer);
            } else if (memcmp (buffer.buffer, bcaddr,6) == 0) { // Broadcast packet
                // Process the packet in case we should reply to it,
                // then send it on over the radio
                memcpy(extrabuf,buffer.buffer,buffer.bytesUsed);
                proc_eth(extrabuf, buffer.bytesUsed, PORT_ETH);
                ampr_queueEth(&buffer);
            } else {    // it was for someone else, send it over radio
                ampr_queueEth(&buffer);
            }
        } else {
            ethBuf_free(&buffer);
        }
    }
}
