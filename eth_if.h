#ifndef ETH_IF_H_
#define ETH_IF_H_

#include <stdint.h>

void vEthIf_task(void* pvParameters);

void ethIf_send(uint8_t* buffer, uint16_t size);

#endif
