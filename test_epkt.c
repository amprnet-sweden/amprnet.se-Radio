/* copyright Gullik Webjörn, SM4FBD 
This is just a test function for the ethernet driver. It uses packets with protocol type
60-06 which was registered to users of DEC machines for their own hacky ethernet protocols 
There is a corresponding packet generator for Linux so that packets can be sent with a 
programmable interval, and a receiver can check that all were transmitted. */

#include <stdint.h>
#include "Ampr-radio.h"
int nxtpkt;
int lost;
void test_epkt(uint8_t *pktbuf,int count) {
    int pktno;
	if((pktbuf[12] == 0x60) && (pktbuf[13] == 0x06)) {
	    pktno = pktbuf[14] * 256 + pktbuf[15];
	    switch(pktno) {
	    case 0:
            lost = 0;
            nxtpkt = 1;
            xprint("Pkt 0 ");
	        break;
	    case 999:
            xprint("Pkt == 999 lost ");
            xprint_int(lost);
            xprint("\n");
	        break;
	    default:
	        if(pktno == nxtpkt) {
	            nxtpkt++;
	        } else {
	             lost++;
	             nxtpkt = pktno + 1;
	        }
	    }

	}
}
