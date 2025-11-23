#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include "Ampr-radio.h"
#include "w5500.h"


uint8_t my_ip[] = {44,5,5,20};
//uint8_t my_ip[] = {192,168,1,10};
uint8_t bcaddr[6] = {255,255,255,255,255,255};

uint16_t checksum(uint16_t * addr, int len) {
     int count = len;
     register uint32_t sum = 0;
     uint16_t answer = 0;
//     xprint("Checksumming ");
//     xprint_int(count);
//     xprint("bytes\n");
     // Sum up 2-byte values until none or only one byte left.
     while (count > 1) {
       sum += *(addr++);
       count -= 2;
     }

     // Add left-over byte, if any.
     if (count > 0) {
       sum += *(uint8_t *) addr;
     }

     // Fold 32-bit sum into 16 bits; we lose information by doing this,
     // increasing the chances of a collision.
     // sum = (lower 16 bits) + (upper 16 bits shifted right 16 bits)
     while (sum >> 16) {
       sum = (sum & 0xffff) + (sum >> 16);
     }

     // Checksum is one's compliment of sum.
     answer = ~sum;

     return (answer);
   }


void arp_reply(uint8_t * buf, int count,char port) {
//    memcpy(&buf[32],my_hwaddr,6);  //set my hw addr
    buf[21] = 2;                    // set reply
    memcpy(&buf[0],&buf[6],6); // copy src to dest
    memcpy(&buf[6], my_hwaddr,6); // use my src address
    memcpy(&buf[32],&buf[22],10); // fill in hw adder and ip addr
    memcpy(&buf[22],my_hwaddr,6);   // and fill my mac
    memcpy(&buf[28], my_ip,4);     // and fill in my address
    if (port == 1) {
      if(EthEna) {
        w5500sendFrame(buf,count);
      }
/*          for(int i=0;i<count;i++) {
                 xprint_xchar(buf[i]);
                 if(i%64 == 63) {
                    xprint("\n");
                 } else {
                    xprint(" ");
                 }
            }
            xprint("\n"); */
    }
    if(port == 2) {
//      xprint("reply to arp via 2");
      my_Q++;
      queue_eth(buf,count,my_Q);
    }
}
void icmp_reply(uint8_t * buf, int count,char port) {
//    uint16_t chk;
    if(buf[34] == 8) {
//        xprint("ICMP Request\n");
        int chk = checksum((uint16_t * ) &buf[34],buf[17] + (buf[16] >> 8));
//        xprint_xchar(chk >> 8);
//        xprint_xchar(chk & 0xff);
//        xprint("\n");
        buf[34] = 0;                   // set echo reply
        memcpy(&buf[0],&buf[6],6);    // copy src to dest
        memcpy(&buf[6], my_hwaddr,6); // use my src address
        memcpy(&buf[30],&buf[26],4);
        memcpy(&buf[26],my_ip,4);
        if(chk >= 0xffff - 0x0800) {
            chk += 0x801;
        } else {
            chk += 0x800;
        }
//        buf[36]=0;
//        buf[37]=0;
//        int chk = checksum((uint16_t * ) &buf[34],buf[17] + (buf[16] >> 8));
//        buf[37] = chk>>8;
//        buf[36] = chk;
//        xprint_xchar(chk >> 8);
//        xprint_xchar(chk & 0xff);
//        xprint("\n");
          buf[36] += 8;
          if(port == 1) {
            if(EthEna) {
              w5500sendFrame(buf,count);
            }
          }
          if(port == 2) {
//              xprint("icmp to poirt 2\n");
              my_Q++;
              queue_eth(buf,count,my_Q);
          }
    }
}
void proc_eth(uint8_t * buffer, int count,char port) {
//	xprint("Our addr\n");
	if(memcmp(buffer,bcaddr,6) == 0) {
/*	    xprint("Bcast\n");
	    xprint("\n"); */
	    if((buffer[12] == 0x08) && (buffer[13] == 0x06)) {
//	        xprint("ARP ");
	        if((buffer[14] == 0) && (buffer[15] == 1)) {
                if(memcmp(&buffer[38],my_ip,4) == 0) {
//                    xprint("ARP ReqOurs\n");
                    arp_reply(buffer, count,port);
                }
	        }
	    }
//	    return;
	}
	if(memcmp(buffer,my_hwaddr,6) == 0) {
//	    xprint("Our addr\n");
        if((buffer[12] == 0x08) && (buffer[13] == 0x00)) {
//          xprint("IP\n");
          switch (buffer[23]) {
          case 1:
//              xprint("ICMP\n");
              icmp_reply(buffer, count, port);
              break;
          default:
              xprint_char(buffer[23]);
              break;
          }
/*          for(int i=0;i<count;i++) {
	        xprint_xchar(buffer[i]);
	        if(i%64 == 63) {
	          xprint("\n");
	        } else {
	          xprint(" ");
	        }
	     } */
//	    xprint("\n");
//	    return;
        }
	}
}
