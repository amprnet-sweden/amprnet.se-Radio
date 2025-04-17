#include <stddef.h>
struct htable {
        char macaddr[6];
        int     ttl;
};


void showheard(void);
void setheard(unsigned char *mac);


