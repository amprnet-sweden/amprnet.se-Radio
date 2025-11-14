#include <FreeRTOS.h>
#include <semphr.h>
#include "Ampr-radio.h"
#include "w5500.h"

void send_log_packet(char *buff) {
    uint8_t tbuf[100];
    int len,slen,timlen;
    for(int i=0;i<6;i++) {
        tbuf[i] = 0xff;
        tbuf[i+6] = my_hwaddr[i];
    }
    tbuf[12] = 0x60;
    tbuf[13] = 0x06;
    tbuf[14] = 0x55;    // code for log test
    timlen = sprintf(&tbuf[16],"T %d.%d ",Runtime/100000,Runtime%100000) + 16;
    memcpy(&tbuf[timlen], buff, strlen(buff)); // copy the message to xmit buffer
    slen = timlen + strlen(buff);
    tbuf[slen] = 0;
    tbuf[15] = slen & 255;
    queue_eth_out(tbuf,slen+16);
}


void xmt_ethernet(void) {
     if(Timer1 == 0) {      // only do this if defer timer timed out
         dequeue_uart();
         if (EthEna == 1) {
           int reclen = dequeue_eth(); // dequeue causes send of packet
/*        if(reclen == 0) { // nothing sent
            sendack(dseg);
            Timer1 = tdelay; */
        }
     }
}

void Net(void) {
//    thisprog = xTaskGetCurrentTaskHandle();
    TaskStatus_t nettask;
    uint8_t *op;
    vTaskGetInfo(NULL, &nettask, NULL, eInvalid);
    op = nettask.pcTaskName;
    memcpy(op,"Net\0",4);
/*    GPIO_setCallback(EINT, &w5500int);
    GPIO_enableInt(EINT);
    int lcount = 0;
    init_ether();
    xSemaphore = xSemaphoreCreateMutex(); */
    int lcount = 0;
    while(1) {
      vTaskDelay(pdMS_TO_TICKS(1));
//      xTaskNotifyWait(0,0,NULL,0);
      if(++lcount == 60000) {
         lcount = 0;
         send_log_packet("TDMA time\n");
//         GPIO_toggle(sigpin2); // just to check loop time
      }
     xmt_ethernet();
  }
}
