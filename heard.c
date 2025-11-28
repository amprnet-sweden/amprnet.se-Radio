#include <string.h>
#include "heard.h"
#include "Ampr-radio.h"

#define htabsize 4

struct htable htab[htabsize];


void showheard(void) {
    xprint("Heard     MAC    TTL\n");
	for (int i=0;i<htabsize;i++) {
//	xprint("MAC ");
	for (int j=0;j<6;j++) {
	    xprint_xchar(htab[i].macaddr[j]);
	    xprint(":");
	}
	xprint(" ");
	xprint_int(htab[i].ttl);
	xprint("\n");
	}
}

void setheard(uint8_t *macaddr) {
	memcpy(&htab[0].macaddr[0],macaddr,6);	//store mac addr
}
