#ifndef AMPR_QUEUE_H_
#define AMPR_QUEUE_H_

#include "ethBuf.h"
#include <stdint.h>

enum
{
    AMPR_QUEUE_NONE = 0,
    AMPR_QUEUE_RADIO,
    AMPR_QUEUE_ETH,
};

typedef struct amprEntry_s {
    uint8_t type;
    // Radio entries don't have any data since the packets are kept in the RFQueue
    // but if additional entry-type-specific data is needed add it to this union.
    union {
        ethBufHandle_t ethHandle;
    };
} amprEntry_t;


void ampr_initQueue();

void ampr_queueEth(ethBufHandle_t* bufferHandle);

void ampr_queueRadioFromISR();

amprEntry_t ampr_dequeue(uint32_t timeout_ms);

#endif /* AMPR_QUEUE_H_ */
