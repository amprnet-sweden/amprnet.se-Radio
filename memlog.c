#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include "Ampr-radio.h"
#ifdef MEMLOG
#ifdef NETLOG
uint8_t lg_ip[] = {0,0,0,0};
#endif
char logbuf[LOGSIZE];
char *logptr;

void loginit(void) {
	bzero(logbuf, LOGSIZE);	// clear out the logger buffer
	logptr = &logbuf[0];
}

void dolog(uint8_t *message, int size, int net) {
    uint8_t secs,millis,len;
    char timefield[10];
    secs = Runtime/1000;
    millis = Runtime % 1000;
    len = sprintf(timefield,"%4d:%03d ",secs,millis);
/*    xprint("log ");
    xprint_int(secs);
    xprint(":");
    xprint_int(millis);
    xprint(" \r\n"); */
	if((logptr + size + len) < &logbuf[LOGSIZE -1]) {
        memcpy(logptr,timefield,len);
        logptr = logptr + len;
        memcpy(logptr,message,size);
		logptr = logptr + size;
	}
}

void showlog(void) {
	xprint(&logbuf[0]);
}
#endif 
