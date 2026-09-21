
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
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include "Ampr-radio.h"
#include "eth_if.h"


uint8_t my_ip[] = {44,5,5,20};                  // program default, saveable
uint8_t bcaddr[6] = {255,255,255,255,255,255};

// calculate an internet checksum
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
uint8_t idledata[64];
void queue_idle_data(void) {
    memcpy(idledata,last_radio,6);
    memcpy(&idledata[6],my_hwaddr,6);
    idledata[12] = 0x60;
    idledata[13] = 0x06;
    for(int i = 14;i<44;i++) {
        idledata[i] = i;    // just fill payload
    }
/*    for(int i=0;i<64;i++) {
           xprint_xchar(idledata[i]);
           if(i%64 == 63) {
              xprint("\n");
           } else {
              xprint(" ");
           }
      }
      xprint("\n"); */
    my_Q++;
//      xprint_char(my_Q);
//      xprint("\n");
    queue_eth(idledata,58,my_Q);  // 44 + 14
}

void send_eth_frame(uint8_t * buffer, uint8_t * dst, uint16_t type, uint8_t * payload, uint32_t len, int port) {
    memcpy(buffer, dst, 6);
    memcpy(&buffer[6], my_hwaddr, 6);
    buffer[12] = type >> 8;
    buffer[13] = type & 0xff;
/*    memcpy(&buffer[14], payload, len);
    if (port == 1) {
      if(EthEna) {
        ethIf_send(buffer,len+14);
      } */
          for(int i=0;i<64;i++) {
                 xprint_xchar(buffer[i]);
                 if(i%64 == 63) {
                    xprint("\n");
                 } else {
                    xprint(" ");
                 }
            }
            xprint("\n");
/*    }
    if(port == 2) {
//      xprint("reply to arp via 2 Q :");
      my_Q++;
//      xprint_char(my_Q);
//      xprint("\n");
      queue_eth(buffer,len+14,my_Q);
    } */

}

void arp_reply(uint8_t * buf, int count,char port) {
//    memcpy(&buf[32],my_hwaddr,6);  //set my hw addr
    buf[21] = 2;                    // set reply
    memcpy(&buf[0],&buf[6],6); // copy src to dest
    memcpy(&buf[6], my_hwaddr,6); // use my src address
    memcpy(&buf[32],&buf[22],10); // fill in hw adder and ip addr
    memcpy(&buf[22],my_hwaddr,6);   // and fill my mac
    memcpy(&buf[28], my_ip,4);     // and fill in my address
    if (port == PORT_ETH) {
      if(EthEna) {
        ethIf_send(buf,count);
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
    if(port == PORT_RADIO) {
//      xprint("reply to arp via 2 Q :");
      my_Q++;
//      xprint_char(my_Q);
//      xprint("\n");
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
          if(port == PORT_ETH) {
            if(EthEna) {
              ethIf_send(buf,count);
            }
          }
          if(port == PORT_RADIO) {
//              xprint("icmp to poirt 2\n");
              my_Q++;
              queue_eth(buf,count,my_Q);
          }
    }
}
#ifdef DHCPC
uint8_t srvaddr[4];
void udp_input(uint8_t * buffer, int count) {
//    uint8_t myaddr[4];
    uint8_t netmask[4];
    uint8_t gateway[4];
    uint32_t sport,dport;
    sport = (buffer[34] << 8 | buffer[35]);
    dport = (buffer[36] << 8 | buffer[37]);
//    xprint("Got udp ");
//    xprint_int(sport);
//    xprint(" ");
//    xprint_int(dport);
//    xprint("\n");
    if((sport == 67) && (dport == 68)) {  // we are getting a dhcp response
        switch (buffer[284]) {
        case 2:
            my_ip[0] = buffer[58];
            my_ip[1] = buffer[59];
            my_ip[2] = buffer[60];
            my_ip[3] = buffer[61];
            srvaddr[0] = buffer[62];
            srvaddr[1] = buffer[63];
            srvaddr[2] = buffer[64];
            srvaddr[3] = buffer[65];
//            dhcp_request(&my_hwaddr);
            int i = 282;    //index to option fields
            while (i < count) {
              uint8_t opt = buffer[i++];
              uint8_t len = buffer[i++];
              if (opt == 01) {
                  netmask[0] = buffer[i];
                  netmask[1] = buffer[i+1];
                  netmask[2] = buffer[i+2];
                  netmask[3] = buffer[i+3];
              }
              if (opt == 03) {
                  gateway[0] = buffer[i];
                  gateway[1] = buffer[i+1];
                  gateway[2] = buffer[i+2];
                  gateway[3] = buffer[i+3];
              }
              i = i+len;
              if(opt == 255)
                  break;
            }
            xprint("Got DHCP addr ");
            xprint_int(my_ip[0]);
            xprint(".");
            xprint_int(my_ip[1]);
            xprint(".");
            xprint_int(my_ip[2]);
            xprint(".");
            xprint_int(my_ip[3]);
            xprint(" from ");
            xprint_int(srvaddr[0]);
            xprint(".");
            xprint_int(srvaddr[1]);
            xprint(".");
            xprint_int(srvaddr[2]);
            xprint(".");
            xprint_int(srvaddr[3]);
            xprint(" Gateway ");
            xprint_int(gateway[0]);
            xprint(".");
            xprint_int(gateway[1]);
            xprint(".");
            xprint_int(gateway[2]);
            xprint(".");
            xprint_int(gateway[3]);
            xprint(" Netmask ");
            xprint_int(netmask[0]);
            xprint(".");
            xprint_int(netmask[1]);
            xprint(".");
            xprint_int(netmask[2]);
            xprint(".");
            xprint_int(netmask[3]);
//           for(int i = 282;i<count;i++) {
//               xprint_xchar(buffer[i]);
//               xprint(" ");
//           }
            xprint("\n");
            dhcp_request(&my_hwaddr);
            break;
        case 5:
//           my_ip[0] = buffer[58];
//           my_ip[1] = buffer[59];
//           my_ip[2] = buffer[60];
//           my_ip[3] = buffer[61];
//           srvaddr[0] = buffer[62];
//           srvaddr[1] = buffer[63];
//           srvaddr[2] = buffer[64];
//           srvaddr[3] = buffer[65];
           xprint("Got DHCP ACK ");
//           xprint_int(my_ip[0]);
//           xprint(".");
//           xprint_int(my_ip[1]);
//           xprint(".");
//           xprint_int(my_ip[2]);
//           xprint(".");
//           xprint_int(my_ip[3]);
//           xprint(" from ");
//           xprint_int(srvaddr[0]);
//           xprint(".");
//           xprint_int(srvaddr[1]);
//           xprint(".");
//           xprint_int(srvaddr[2]);
//           xprint(".");
//           xprint_int(srvaddr[3]);
           xprint("\n");
           break;
         default:
             xprint("DHCP ");
             xprint_char(buffer[284]);
             xprint(" ?? \n");
             break;
        }

    }
}
#endif
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
          case 17:
/*              for(int i=0;i<count;i++) {
                xprint_xchar(buffer[i]);
                if(i%64 == 63) {
                  xprint("\n");
                } else {
                  xprint(" ");
                }
              }
              xprint("\n"); */
#ifdef DHCPC
              udp_input(buffer,count);
#endif
              break;
          default:
/*              xprint("To us : ");
              xprint_char(buffer[23]);
              xprint("\n"); */
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
/*        if((buffer[12] == 0x60) && (buffer[13] == 0x06)) { // debug that we got idle packet to propagate a type 7 packet when wueue is empty
            if(debug & 32) {
              xprint("idle To me\n");
            }
        } */
	}
}
#ifdef DHCPC
/*
 * http://www.tcpipguide.com/free/t_DHCPMessageFormat.htm
 */
typedef u_int32_t ip4_t;

#define DHCP_CHADDR_LEN 16
#define DHCP_SNAME_LEN  64
#define DHCP_FILE_LEN   128
#define IPPROTO_UDP 17
#define htons(x) (uint16_t)((((uint16_t)(x) & 0x00ff) << 8) | \
                                   (((uint16_t)(x) & 0xff00) >> 8))
//# define htonl(x)    (((x >> 24) & 0x000000FF) | ((x >> 8) & 0x0000FF00) | ((x << 8) & 0x00FF0000) | ((x << 24) & 0xFF000000))

typedef struct ether_header
{
    uint8_t ether_dhost[6];
    uint8_t ether_shost[6];
    uint16_t ether_type;
}ether;

typedef struct ip
{
    uint8_t ip_ivhl;
    uint8_t ip_tos;
    uint16_t ip_len;
    uint16_t ip_id;
    uint16_t ip_off;
    uint8_t ip_ttl;
    uint8_t ip_p;
    uint16_t ip_sum;
    uint32_t ip_src;
    uint32_t ip_dst;
}ip;
typedef struct udphdr
{
    uint16_t uh_sport;
    uint16_t uh_dport;
    uint16_t uh_ulen;
    uint16_t uh_sum;
}udp;
typedef struct dhcp
{
    uint8_t    opcode;
    uint8_t    htype;
    uint8_t    hlen;
    uint8_t    hops;
    uint32_t   xid;
    uint16_t   secs;
    uint16_t   flags;
    ip4_t       ciaddr;
    ip4_t       yiaddr;
    ip4_t       siaddr;
    ip4_t       giaddr;
    uint8_t    chaddr[DHCP_CHADDR_LEN];
    char        bp_sname[DHCP_SNAME_LEN];
    char        bp_file[DHCP_FILE_LEN];
    uint32_t    magic_cookie;
    uint8_t    bp_options[0];
} dhcp_t;

#define DHCP_BOOTREQUEST                    1
#define DHCP_BOOTREPLY                      2
#define DHCP_REQUEST                        3

#define DHCP_HARDWARE_TYPE_10_EHTHERNET     1

#define MESSAGE_TYPE_PAD                    0
#define MESSAGE_TYPE_REQ_SUBNET_MASK        1
#define MESSAGE_TYPE_ROUTER                 3
#define MESSAGE_TYPE_DNS                    6
#define MESSAGE_TYPE_DOMAIN_NAME            15
#define MESSAGE_TYPE_REQ_IP                 50
#define MESSAGE_TYPE_DHCP                   53
#define MESSAGE_TYPE_PARAMETER_REQ_LIST     55
#define MESSAGE_TYPE_HOSTNAME               12
#define MESSAGE_TYPE_SERVER                 54
#define MESSAGE_TYPE_END                    255

#define DHCP_OPTION_DISCOVER                1
#define DHCP_OPTION_OFFER                   2
#define DHCP_OPTION_REQUEST                 3
#define DHCP_OPTION_PACK                    4

#define ETHER_ADDR_LEN 6
#define DHCP_SERVER_PORT    67
#define DHCP_CLIENT_PORT    68
//#define DHCP_MAGIC_COOKIE   0x63825363
#define DHCP_MAGIC_COOKIE_INV   0x63538263

/*
 * Return checksum for the given data.
 * Copied from FreeBSD
 */
static unsigned short
in_cksum(unsigned short *addr, int len)
{
    register int sum = 0;
    uint16_t answer = 0;
    register uint16_t *w = addr;
    register int nleft = len;
    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }
    /* mop up an odd byte, if necessary */
    if (nleft == 1)
    {
        *(uint8_t *)(&answer) = *(uint8_t *) w;
        sum += answer;
    }
    /* add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff);     /* add hi 16 to low 16 */
    sum += (sum >> 16);             /* add carry */
    answer = ~sum;              /* truncate to 16 bits */
    return (answer);
}

/*
 * Ethernet output handler - Fills appropriate bytes in ethernet header
 */
static int
ether_output(uint8_t *frame, uint8_t *mac, int len)
{
//    int result;
//    int i;
    struct ether_header *eframe = (struct ether_header *)frame;

    memcpy(eframe->ether_shost, mac, ETHER_ADDR_LEN);
    memset(eframe->ether_dhost, -1,  ETHER_ADDR_LEN);
    eframe->ether_type = 0x0008; //htons(ETHERTYPE_IP);

    len += sizeof(struct ether_header);

    /* Send the packet on wire */
/*    for(i=0;i<len;i++) {
        xprint_xchar(frame[i]);
        if((i % 40 ) == 0) {
            xprint("\n");
        } else xprint(" ");

    } */


//    result = pcap_inject(pcap_handle, frame, len);
//    PRINT(VERBOSE_LEVEL_DEBUG, "Send %d bytes\n", result);
//    if (result <= 0)
//        pcap_perror(pcap_handle, "ERROR:");
    return(len);
}

/*
 * IP Output handler - Fills appropriate bytes in IP header
 */
static void
ip_output(struct ip *ip_header, int *len)
{
    *len += sizeof(struct ip);

//    ip_header->ip_hl = 5;
//    ip_header->ip_v = IPVERSION;
    ip_header->ip_ivhl = 0x45;
    ip_header->ip_tos = 0x00;
    ip_header->ip_len = htons(*len);
    ip_header->ip_id = htons(0x1234);
    ip_header->ip_off = 0x0040;
    ip_header->ip_ttl = 0;
    ip_header->ip_p = IPPROTO_UDP;
    ip_header->ip_sum = 0;
    ip_header->ip_src = 0;
    ip_header->ip_dst = 0xFFFFFFFF;

    ip_header->ip_sum = in_cksum((unsigned short *) ip_header, sizeof(struct ip));
}

/*
 * UDP output - Fills appropriate bytes in UDP header
 */
static void
udp_output(struct udphdr *udp_header, int *len)
{
    if (*len & 1)
        *len += 1;
    *len += sizeof(struct udphdr);

    udp_header->uh_sport = htons(DHCP_CLIENT_PORT);
    udp_header->uh_dport = htons(DHCP_SERVER_PORT);
    udp_header->uh_ulen = htons(*len);
    udp_header->uh_sum = 0;
}
/*
 * DHCP output - Just fills DHCP_BOOTREQUEST
 */
static void
dhcp_output(dhcp_t *dhcp, u_int8_t *mac, int *len)
{
    *len += sizeof(dhcp_t);
    memset(dhcp, 0, sizeof(dhcp_t));

    dhcp->opcode = DHCP_BOOTREQUEST;
//    dhcp->opcode = op;
    dhcp->htype = DHCP_HARDWARE_TYPE_10_EHTHERNET;
    dhcp->hlen = 6;
    memcpy(dhcp->chaddr, mac, DHCP_CHADDR_LEN);

//    dhcp->magic_cookie = htonl(DHCP_MAGIC_COOKIE);
    dhcp->magic_cookie = (DHCP_MAGIC_COOKIE_INV);
}
/*
 * Adds DHCP option to the bytestream
 */
static int
fill_dhcp_option(u_int8_t *packet, u_int8_t code, u_int8_t *data, u_int8_t len)
{
    packet[0] = code;
    packet[1] = len;
    memcpy(&packet[2], data, len);

    return len + (sizeof(u_int8_t) * 2);
}

/*
 * Fill DHCP options
 */
#ifdef N536RADIO
uint8_t hostname[16] = {"Amprnet-Radio-R2"};
#else
uint8_t hostname[16] = {"Amprnet-Radio-R1"};
#endif

unsigned char packet[512];
/*
 * Fill DHCP options
 */
static int
fill_dhcp_discovery_options(dhcp_t *dhcp)
{
    int len = 0;
    u_int32_t req_ip;
    u_int8_t parameter_req_list[] = {MESSAGE_TYPE_REQ_SUBNET_MASK, MESSAGE_TYPE_ROUTER, MESSAGE_TYPE_DNS, MESSAGE_TYPE_DOMAIN_NAME};
    u_int8_t option;

    option = DHCP_OPTION_DISCOVER;
    len += fill_dhcp_option(&dhcp->bp_options[len], MESSAGE_TYPE_DHCP, &option, sizeof(option));
    req_ip = (0);
    len += fill_dhcp_option(&dhcp->bp_options[len], MESSAGE_TYPE_REQ_IP, (u_int8_t *)&req_ip, sizeof(req_ip));
    len += fill_dhcp_option(&dhcp->bp_options[len], MESSAGE_TYPE_PARAMETER_REQ_LIST, (u_int8_t *)&parameter_req_list, sizeof(parameter_req_list));
//    len += fill_dhcp_option(&dhcp->bp_options[len], MESSAGE_TYPE_HOSTNAME, (u_int8_t *)&version, strlen(version));
    len += fill_dhcp_option(&dhcp->bp_options[len], MESSAGE_TYPE_HOSTNAME, (u_int8_t *)&hostname, sizeof(hostname));
    len += fill_dhcp_option(&dhcp->bp_options[len], MESSAGE_TYPE_SERVER, (uint8_t *)&srvaddr, 4);
    option = 0;
    len += fill_dhcp_option(&dhcp->bp_options[len], MESSAGE_TYPE_END, &option, sizeof(option));

    return len;
}
/*
 * Send DHCP REQUEST packet
 */
#ifdef DHCPC
int dhcp_request(uint8_t *mac)
{
    int len = 0;
//    int i;
    struct udphdr *udp_header;
    struct ip *ip_header;
    dhcp_t *dhcp;

//    PRINT(VERBOSE_LEVEL_INFO, "Sending DHCP_DISCOVERY");

    ip_header = (struct ip *)(packet + sizeof(struct ether_header));
    udp_header = (struct udphdr *)(((char *)ip_header) + sizeof(struct ip));
    dhcp = (dhcp_t *)(((char *)udp_header) + sizeof(struct udphdr));

    len = fill_dhcp_discovery_options(dhcp);
    dhcp_output(dhcp, mac, &len);
    packet[284] = 3;                // set type = request, this should really be done by parsing packet and finding option
    packet[287] = my_ip[0];         // but since WE made the discover packet we KNOW the offsets are correct
    packet[288] = my_ip[1];
    packet[289] = my_ip[2];
    packet[290] = my_ip[3];
    udp_output(udp_header, &len);
    ip_output(ip_header, &len);
    len = ether_output(packet, mac, len);
/*    for(i=0;i<len;i++) {
        xprint_xchar(packet[i]);

    } */
    if(EthEna) {
      ethIf_send(packet,len);
    }
    my_Q++;
    queue_eth(packet,len,my_Q);
    return 0;
}
#endif
/*
 * Send DHCP DISCOVERY packet
 */
int dhcp_discovery(uint8_t *mac)
{
    int len = 0;
//    int i;
    struct udphdr *udp_header;
    struct ip *ip_header;
    dhcp_t *dhcp;

//    PRINT(VERBOSE_LEVEL_INFO, "Sending DHCP_DISCOVERY");
    bzero(packet,sizeof(packet));       // start with empty packet
    ip_header = (struct ip *)(packet + sizeof(struct ether_header));
    udp_header = (struct udphdr *)(((char *)ip_header) + sizeof(struct ip));
    dhcp = (dhcp_t *)(((char *)udp_header) + sizeof(struct udphdr));

    len = fill_dhcp_discovery_options(dhcp);
    dhcp_output(dhcp, mac, &len);
    udp_output(udp_header, &len);
    ip_output(ip_header, &len);
    len = ether_output(packet, mac, len);
/*    for(i=0;i<len;i++) {
        xprint_xchar(packet[i]);

    } */
    if(EthEna) {
      ethIf_send(packet,len);
    }
    my_Q++;
    queue_eth(packet,len,my_Q);
    return 0;
}


#endif
