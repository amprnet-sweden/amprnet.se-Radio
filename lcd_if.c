#include <FreeRTOS.h>
#include <task.h>
#include "Ampr-radio.h"
#include "lcd_if.h"
#include "ssd1306.h"
int position;
char x;
char y;
char tmpbuf[8];
void vLcdIf_task(void* pvParameters)
{
    /* Block for 500ms. */
    const TickType_t xDelay = 200 / portTICK_PERIOD_MS;
//    const TickType_t yDelay = 2000 / portTICK_PERIOD_MS;
//#if defined LCD
    vTaskDelay( xDelay );
#ifdef OLED
    LcdEna = SSD1306_Begin();
#else
    LcdEna = SSD1306_Begin();
#endif
    if(LcdEna == 0) {
        xprint("SSD1306 detected\n");
        delay(2);
#ifdef OLED
        SSD1306_ClearScreen (); // clear screen
//        SSD1306_InverseScreen(0x3c);
        SSD1306_UpdateScreen (SSD1306_ADDR);                            // update
//        SSD1306_DrawLine (0, MAX_X, 4, 4);                              // draw line
//        SSD1306_DrawChar (65);
/*        SSD1306_DrawPixel (1,1);
        SSD1306_DrawPixel (33,1);
        SSD1306_DrawPixel (64,1);
        SSD1306_DrawPixel (96,1);
        SSD1306_DrawPixel (126,1);
        SSD1306_DrawPixel (1,62);
        SSD1306_DrawPixel (33,62);
        SSD1306_DrawPixel (64,62);
        SSD1306_DrawPixel (96,62);
        SSD1306_DrawPixel (126,62); */
//        SSD1306_DrawPixel (33,64);
//        SSD1306_DrawPixel (33,126);
//        SSD1306_DrawPixel (63,1);
//        SSD1306_DrawPixel (63,64);
//        SSD1306_DrawPixel (63,126);
        position = SSD_GetPosition();
        x = position & 0x7f;
        y = position >> 7;
        xprint("Pos ");
        xprint_char(x);
        xprint(" ");
        xprint_char(y);
        xprint("\n");
        SSD1306_SetPosition (7, 1);                                     // set position
        SSD1306_DrawString("-85");
        SSD1306_DrawBlock(0x3f);
        SSD1306_DrawBlock(0x3f);
        SSD1306_DrawBlock(0x3f);
        SSD1306_DrawBlock(0x3f);
//        SSD1306_DrawLine (3, 3,  60,  3);
//        SSD1306_DrawLine (3, 3,  3,  120);
//        SSD1306_DrawLine (60, 3,  60,  120);
//        SSD1306_DrawLine (3, 3,  60,  3);
//        SSD1306_DrawString ("SSD1306 OLED DRIVER");                     // draw string
//        SSD1306_DrawLine (0, MAX_X, 18, 18);                            // draw line
        SSD1306_SetPosition (3, 3);                                    // set position
        SSD1306_DrawString ("Amprnet.se 23 cm TRX");                                // draw string
        SSD1306_SetPosition (33, 5);                                    // set position
        SSD1306_DrawString ("25-aug 2026"); // draw string
        SSD1306_SetPosition(1,6);
        SSD1306_DrawString (version); // draw string

        //        SSD1306_DrawChar(65);
        SSD1306_UpdateScreen (SSD1306_ADDR);                            // update
//        vTaskDelay( yDelay );
//        SSD1306_ClearScreen (); // clear screen

//        SSD1306_InverseScreen (SSD1306_ADDR);
//          SSD1306_NormalScreen (SSD1306_ADDR);


#else
        LCD_Print(version);
#endif
//        delay(1);
    } else {
        xprint("NO LCD");
    }
//#endif

    for( ;; )
    {
        vTaskDelay( xDelay );
        SSD1306_SetPosition (7, 1);                                     // set position
        if (LcdEna == 0) {
        	if((signed char) rssi > -100) {
        	  sprintf(tmpbuf,"%2d",(signed char) rssi);
              SSD1306_DrawString (tmpbuf); // draw string
        	} else {
                SSD1306_DrawString ("NaN"); // draw string
        	}
            SSD1306_UpdateScreen (SSD1306_ADDR);                            // update
//                   smeter(rssi);
        }
    }
}
