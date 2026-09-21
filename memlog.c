
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

void dolog(char *message, int size, int net) {
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
