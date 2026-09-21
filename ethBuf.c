
/*
 * Copyright (C) 2024 AMPRNet Sweden
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */ 
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
