/*
 * lcd.c
 *
 *  Created on: Mar 7, 2025
 *      Author: webjorn
 */
#include "ti_drivers_config.h"
#include "Ampr-radio.h"

#define LCD_BACKLIGHT          0x08
#define LCD_NOBACKLIGHT        0x00
#define LCD_FIRST_ROW          0x80
#define LCD_SECOND_ROW         0xC0
#define LCD_THIRD_ROW          0x94
#define LCD_FOURTH_ROW         0xD4
#define LCD_CLEAR              0x01
#define LCD_RETURN_HOME        0x02
#define LCD_ENTRY_MODE_SET     0x04
#define LCD_CURSOR_OFF         0x0C
#define LCD_UNDERLINE_ON       0x0E
#define LCD_BLINK_CURSOR_ON    0x0F
#define LCD_MOVE_CURSOR_LEFT   0x10
#define LCD_MOVE_CURSOR_RIGHT  0x14
#define LCD_TURN_ON            0x0C
#define LCD_TURN_OFF           0x08
#define LCD_SHIFT_LEFT         0x18
#define LCD_SHIFT_RIGHT        0x1E
#ifndef LCD_TYPE
#define LCD_TYPE 2           // 0=5x7, 1=5x10, 2=2 lines
#endif

uint8_t backlight_val = LCD_BACKLIGHT;

//#define LCD
#include <ti/drivers/I2C.h>
// Define name for an index of an I2C bus
#define SENSORS 0
// Define the target address of device on the SENSORS bus
#define LCD_ADDR 0x27
I2C_Params LCDparams;
I2C_Handle i2cHandle;
I2C_Transaction transaction = {0};
uint8_t RS;
char rdata[2];
char wdata[2];
void Expander_Write(uint8_t value) {
    wdata[0] = value;
    I2C_transfer(i2cHandle, &transaction);
//    xprint("I2c out ");
//    xprint_xchar(value>>4);
//    xprint(" E ");
//    xprint_char(value & 0x0f);
//    xprint("\n");
}

void LCD_Write_Nibble(uint8_t n) {
  n |= RS | 0x8;
  Expander_Write(n);
  Expander_Write(n | 0x04); // gtoggle e bit
//  delay(1);
  Expander_Write(n & 0xFB); // toggle e bit
//  delay(50);
}

void LCD_Cmd(uint8_t Command) {
  RS = 0;
  LCD_Write_Nibble(Command & 0xF0);
  LCD_Write_Nibble((Command << 4) & 0xF0);
}

void LCD_Goto(uint8_t col, uint8_t row) {
  switch(row) {
    case 2:
      LCD_Cmd(0xC0 + col-1);
      break;
    case 3:
      LCD_Cmd(0x94 + col-1);
      break;
    case 4:
      LCD_Cmd(0xD4 + col-1);
    break;
    default:      // case 1:
      LCD_Cmd(0x80 + col-1);
  }
}

void LCD_Out(uint8_t LCD_Char){
  RS = 1;
  LCD_Write_Nibble(LCD_Char & 0xF0);
  LCD_Write_Nibble((LCD_Char << 4) & 0xF0);
}



void lcd_test(void) {
        int i;
    char ident[] = {"AmprRadio V0.92e" };
    for(i=0;i< sizeof(ident);i++) {
        LCD_Out(ident[i]);
    }
}
void LCD_Print(char *string) {
    while(*string !=0 ) {
        LCD_Out(*string++);
    }
}
void lcd_putc(char ch) {
//        int i;
        LCD_Out(ch);
}



int LCD_Begin(void) {
    // One-time init of I2C driver
    I2C_init();
    // initialize optional I2C bus parameters
    I2C_Params_init(&LCDparams);
    LCDparams.bitRate = I2C_400kHz;
    // Open I2C bus for usage
    i2cHandle = I2C_open(CONFIG_I2C_0, &LCDparams);
    if (i2cHandle == NULL) {
        // Error opening I2C
        while (1) {}
    }
    // Initialize target address of transaction
    transaction.targetAddress = LCD_ADDR;
    // Read from I2C target device
    transaction.readBuf = &rdata;
    transaction.writeBuf =&wdata;
    transaction.readCount = 0; //sizeof(rdata);
    transaction.writeCount = 1;
    Expander_Write(0);
    if(transaction.status != 0) {
        return(transaction.status);
    }
    LCD_Cmd(3);
//    delay(5);
    LCD_Cmd(3);
//    delay(5);
    LCD_Cmd(3);
//    delay(5);
    LCD_Cmd(LCD_RETURN_HOME);
//    delay(5);
    LCD_Cmd(0x20 | (LCD_TYPE << 2));
//    delay(50);
    LCD_Cmd(LCD_TURN_ON);
//    delay(50);
    LCD_Cmd(LCD_CLEAR);
//    delay(50);
    LCD_Cmd(LCD_ENTRY_MODE_SET | LCD_RETURN_HOME);
//    delay(50);
    return(transaction.status);
}

void Backlight() {
  backlight_val = LCD_BACKLIGHT;
  Expander_Write(0);
}

void noBacklight() {
  backlight_val = LCD_NOBACKLIGHT;
  Expander_Write(0);
}
/*
// CCS C driver code for I2C LCDs (HD44780 compliant controllers)
// http://simple-circuit.com/






void LCD_Write_Nibble(unsigned int8 n);
void LCD_Cmd(unsigned int8 Command);
void LCD_Goto(unsigned int8 col, unsigned int8 row);
void LCD_Out(unsigned int8 LCD_Char);
void LCD_Begin(unsigned int8 _i2c_addr);
void Backlight();
void noBacklight();
void Expander_Write(unsigned int8 value);




*/
