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
        if(reclen > 0) {
            buffer.bytesUsed = reclen;
            ampr_queueEth(&buffer);
        } else {
            ethBuf_free(&buffer);
        }
    }
}
