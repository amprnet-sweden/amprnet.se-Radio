//#include "lcd.h"
#include "oled.h"
char smeter_strings[12][18] = {
			"-95>            \0",
			"-90=>           \0",
			"-85==>          \0",
			"-80===>         \0",
			"-75====>        \0",
			"-70=====>       \0",
			"-65======>      \0",
			"-60  Strong >   \0",
			"-55 Stronger >  \0",
			"-50 Very Strong \0",
			"-45 !----------!\0",
			"-40 ++++++++++++\0"};
void smeter(signed char rssi) {
	signed char i = (rssi+95)/5;
	if (i<0) i=0;
	if(i>11) i = 11;
#ifdef OLED
#else
//    LCD_Goto(1,2);
//	LCD_Print(smeter_strings[i]);
#endif
}
