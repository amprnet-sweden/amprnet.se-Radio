#include <stdint.h>
struct htable {
        char macaddr[6];
        int     ttl;
};
/* typedef struct htable {
    char macaddr[6];
    int  ttl;
} htable; */


extern void showheard(void);
extern void setheard(uint8_t *macaddr);


