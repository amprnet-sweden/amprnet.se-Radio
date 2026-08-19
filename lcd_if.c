#include <FreeRTOS.h>
#include "Ampr-radio.h"
#include "lcd_if.h"
void vLcdIf_task(void* pvParameters) 
{
    /* Block for 500ms. */
    const TickType_t xDelay = 200 / portTICK_PERIOD_MS;
#if defined LCD
    LcdEna = LCD_Begin();
    if(LcdEna == 0) {
        xprint("LCD ON");
        delay(1);
        LCD_Print(version);
        delay(1);
    } else {
        xprint("NO LCD");
    }
#endif

    for( ;; )
    {
        vTaskDelay( xDelay );
        if (LcdEna == 0) {
                   smeter(rssi);
        }
    }
}
