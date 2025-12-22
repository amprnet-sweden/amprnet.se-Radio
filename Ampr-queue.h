#ifndef AMPR_QUEUE_H_
#define AMPR_QUEUE_H_

#include "ethBuf.h"
#include <stdint.h>

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
void ampr_queueRadioTXFromISR();

amprEntry_t ampr_dequeueRadio(uint32_t timeout_ms);

ethBufHandle_t ampr_dequeueEth();

#endif /* AMPR_QUEUE_H_ */
