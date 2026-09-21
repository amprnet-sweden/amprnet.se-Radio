
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
#ifndef ETHBUF_H_
#define ETHBUF_H_

#include <stdint.h>

#define EBSIZE 1518 // TODO size of pktbuf, 4 bytes overhead?

#ifdef CC1314R10
//#define EBCOUNT 30
#else
//#define EBCOUNT 22
#endif

typedef struct ethBufHandle_s
{
    uint8_t* buffer;
    uint16_t bytesUsed;
    uint8_t packetNumber;
} ethBufHandle_t;

// Get a free buffer.
// If no free buffer is available, the handle's buffer pointer will be NULL.
// Do not call from an ISR.
void ethBuf_get(ethBufHandle_t* buf);

// Return a buffer to the free pool.
// Do not call from an ISR.
void ethBuf_free(ethBufHandle_t* buf);

#endif /* ETHBUF_H_ */
