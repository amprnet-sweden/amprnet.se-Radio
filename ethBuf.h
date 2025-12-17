#ifndef ETHBUF_H_
#define ETHBUF_H_

#include <stdint.h>

#define EBSIZE 1518 // TODO size of pktbuf, 4 bytes overhead?

#ifdef CC1314R10
#define EBCOUNT 30
#else
#define EBCOUNT 22
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
