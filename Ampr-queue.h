
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
#ifndef AMPR_QUEUE_H_
#define AMPR_QUEUE_H_

#include "ethBuf.h"
#include <stdint.h>
#include <stdbool.h>

enum
{
    AMPR_QUEUE_NONE = 0,
    AMPR_QUEUE_RX_DATA,
    AMPR_QUEUE_TX_SLOT,
};

typedef struct amprEntry_s {
    uint8_t type;
} amprEntry_t;


void ampr_initQueue();

void ampr_queueEth(ethBufHandle_t* bufferHandle);

void ampr_queueRadioRXFromISR();
void ampr_queueRadioTX();

amprEntry_t ampr_dequeueRadio(uint32_t timeout_ms);

ethBufHandle_t ampr_dequeueEth();

bool ampr_ethQueueEmpty();

#endif /* AMPR_QUEUE_H_ */
