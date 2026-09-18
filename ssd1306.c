/**
 * --------------------------------------------------------------------------------------+
 * @brief       SSD1306 OLED Driver
 * --------------------------------------------------------------------------------------+
 *              Copyright (C) 2020 Marian Hrinko.
 *              Written by Marian Hrinko (mato.hrinko@gmail.com)
 *
 * @author      Marian Hrinko
 * @date        06.10.2020
 * @update      06.12.2022
 * @file        ssd1306.c
 * @version     2.0.0
 * @tested      AVR Atmega328p
 *
 * @depend      ssd1306.h
 * --------------------------------------------------------------------------------------+
 * @descr       Version 1.0.0 -> applicable for 1 display
 *              Version 2.0.0 -> rebuild to 'cacheMemLcd' array
 *              Version 3.0.0 -> simplified alphanumeric version for 1 display
 * --------------------------------------------------------------------------------------+
 * @usage       Basic Setup for OLED Display
 */
 
// @includes
#include "ssd1306.h"
#include "ti_drivers_config.h"
#include "Ampr-radio.h"
#include "oled.h"

// @const List of init commands with arguments by Adafruit
// @link https://github.com/adafruit/Adafruit_SSD1306
//const uint8_t INIT_SSD1306_ADAFRUIT[] __attribute__ ((section(''.text''),used)) = {
 const uint8_t INIT_SSD1306_ADAFRUIT[]  = {
  17,                                                             // number of initializers
  SSD1306_DISPLAY_OFF, 0,                                         // 0xAE / Set Display OFF
  SSD1306_SET_OSC_FREQ, 1, 0x80,                                  // 0xD5 / 0x80 => D=1; DCLK = Fosc / D <=> DCLK = Fosc
  SSD1306_SET_MUX_RATIO, 1, 0x3F,                                 // 0xA8 / 0x3F (64MUX) for 128 x 64 version
                                                                  //      / 0x1F (32MUX) for 128 x 32 version
  SSD1306_DISPLAY_OFFSET, 1, 0x00,                                // 0xD3
  SSD1306_SET_START_LINE, 0,                                      // 0x40
  SSD1306_SET_CHAR_REG, 1, 0x14,                                  // 0x8D / Enable charge pump during display on
  SSD1306_MEMORY_ADDR_MODE, 1, 0x00,                              // 0x20 / Set Memory Addressing Mode
                                                                  // 0x00 / Horizontal Addressing Mode
                                                                  // 0x01 / Vertical Addressing Mode
                                                                  // 0x02 /  Page Addressing Mode (RESET)
  SSD1306_SEG_REMAP_OP, 0,                                        // 0xA0 / remap 0xA1
  SSD1306_COM_SCAN_DIR_OP, 0,                                     // 0xC8
  SSD1306_COM_PIN_CONF, 0, 0x02,                                  // 0xDA / 0x12 - Disable COM Left/Right remap, Alternative COM pin configuration
                                                                  //        0x12 - for 128 x 64 version
                                                                  //        0x02 - for 128 x 32 version
  SSD1306_SET_CONTRAST, 1, 0x8F,                                  // 0x81 / 0x8F - reset value (max 0xFF)
  SSD1306_SET_PRECHARGE, 1, 0xc2,                                 // 0xD9 / higher value less blinking
                                                                  //        0xC2, 1st phase = 2 DCLK,  2nd phase = 13 DCLK
  SSD1306_VCOM_DESELECT, 1, 0x40,                                 // 0xDB / Set V COMH Deselect, reset value 0x22 = 0,77xUcc
  SSD1306_DIS_ENT_DISP_ON, 0,                                     // 0xA4
  SSD1306_DIS_NORMAL, 0,                                          // 0xA6
  SSD1306_DEACT_SCROLL, 0,                                        // 0x2E
  SSD1306_DISPLAY_ON, 0                                           // 0xAF / Set Display ON  
};

// @const uint8_t - List of init commands according to datasheet SSD1306
const uint8_t INIT_SSD1306[]  = {
  19,                                                             // number of initializers
  //SSD1306_RESET, 0,                                               // 0xE4 = Software Reset?
  SSD1306_DISPLAY_OFF, 0,                                         // 0xAE, Set Display OFF
  SSD1306_SET_MUX_RATIO, 1, 0x3F,                                 // 0xA8, 0x3F for 128 x 64 version (64MUX)                                                              //     , 0x1F for 128 x 32 version (32MUX)
  SSD1306_MEMORY_ADDR_MODE, 1, 0x00,                              // 0x20 = Set Memory Addressing Mode
                                                                  // 0x00, Horizontal Addressing Mode
                                                                  // 0x01, Vertical Addressing Mode
                                                                  // 0x02, Page Addressing Mode (RESET)
  SSD1306_SET_START_LINE, 0,                                      // 0x40
  SSD1306_DISPLAY_OFFSET, 1, 0x00,                                // 0xD3
  SSD1306_SEG_REMAP_OP, 0,                                        // 0xA0 / remap 0xA1
  SSD1306_COM_SCAN_DIR_OP, 0,                                     // 0xC0 / remap 0xC8
  SSD1306_COM_PIN_CONF, 1, 0x12,                                  // 0xDA, 0x12 - Disable COM Left/Right remap, Alternative COM pin configuration
                                                                  //       0x12 - for 128 x 64 version
                                                                  //       0x02 - for 128 x 32 version
  SSD1306_SET_CONTRAST, 1, 0x7F,                                  // 0x81, 0x7F - reset value (max 0xFF)
  SSD1306_DIS_ENT_DISP_ON, 0,                                     // 0xA4
  SSD1306_DIS_NORMAL, 0,                                          // 0xA6
  SSD1306_SET_OSC_FREQ, 1, 0x80,                                  // 0xD5, 0x80 => D=1; DCLK = Fosc / D <=> DCLK = Fosc
  SSD1306_SET_PRECHARGE, 1, 0xc2,                                 // 0xD9, higher value less blinking
                                                                  // 0xC2, 1st phase = 2 DCLK,  2nd phase = 13 DCLK
  SSD1306_VCOM_DESELECT, 1, 0x20,                                 // Set V COMH Deselect, reset value 0x22 = 0,77xUcc
  SSD1306_SET_CHAR_REG, 1, 0x14,                                  // 0x8D, Enable charge pump during display on
  SSD1306_DEACT_SCROLL, 0,                                        // 0x2E
  SSD1306_SET_COLUMN_ADDR, 2, START_COLUMN_ADDR, END_COLUMN_ADDR, // 0x21, Specifies column start address and end address / accord. mARTi-null #16
  SSD1306_SET_PAGE_ADDR, 2, START_PAGE_ADDR, END_PAGE_ADDR,       // 0x22, Specifies page start address and end address / accord. mARTi-null #16
  SSD1306_DISPLAY_ON, 0                                           // 0xAF, Set Display ON
};
const uint8_t INIT_SSD1306_OWN[]  = {
		15,
		  SSD1306_DISPLAY_OFF, 3, 0xd5,0x80,0xa8,                          // 0xAE / Set Display OFF
//		  SSD1306_SET_MUX_RATIO, 1, 0x3F,
		  0x3f,0,// 0xA8, 0x3F for 128 x 64 version (64MUX)
		  SSD1306_DISPLAY_OFFSET, 3, 0x00,0x40,0x8d,
		  0x14,0,
		  SSD1306_MEMORY_ADDR_MODE, 3, 0x00,0xa1,0xc8,                                // 0x20
		  SSD1306_COM_PIN_CONF, 0,
		  0x12,0,
		  SSD1306_SET_CONTRAST, 0,
		  0xcf,0,
		  0xd9,0,
		  0xf1,0,
//		  0xdb,5,0x40,0xa4,0xa6,0x2e,0xaf
		  SSD1306_VCOM_DESELECT, 5, 0x40,0xa4,0xa6,0x2e,0xaf,                   // 0xDB / Set V COMH Deselect, reset value 0x22 = 0,77xUcc
		  SSD1306_SET_PAGE_ADDR, 3, START_PAGE_ADDR, 0xff/* END_PAGE_ADDR */,0x21,       // 0x22, Specifies page start address and end address / accord. mARTi-null #16
		  0,0,
		  127,0
};

// @var array Chache memory Lcd 8 * 128 = 1024
static char cacheMemLcd[CACHE_SIZE_MEM];

/**
 * +------------------------------------------------------------------------------------+
 * |== PRIVATE FUNCTIONS ===============================================================|
 * +------------------------------------------------------------------------------------+
 */

/**
 * @brief   SSD1306 Init
 *
 * @param   uint8_t address
 *
 * @return  uint8_t
 */
#include <ti/drivers/I2C.h>

I2C_Params OLEDparams;
I2C_Handle OLEDHandle;
I2C_Transaction transaction = {0};
char rdata[8];
char wdata[40];
void OLEDExpander_Write(uint8_t value) {
    wdata[0] = value;
    I2C_transfer(OLEDHandle, &transaction);
}

uint8_t SSD1306_Init (uint8_t address)
{ 
//	  const uint8_t * list = INIT_SSD1306;
  uint8_t * list = &INIT_SSD1306_OWN;
  uint8_t status = INIT_STATUS;                                   // init status
  uint8_t arguments,cmdbyte,bytecnt;
  uint8_t commands = *list++;

  xprint("1306 init ");
  xprint_char(commands);
  xprint(" ccmds\n");
  bytecnt = 0;
  // -------------------------------------------------------------------------------------
  while (commands--) {
    // Command
    // -----------------------------------------------------------------------------------
//	    status = SSD1306_Send_Command (pgm_read_byte(list++));
	  cmdbyte = *list++;
	  arguments = *list++;
//	  xprint("cmd ");
//	  xprint_xchar(cmdbyte);
	  wdata[0] = 0;
	  wdata[1] = cmdbyte;
	  bytecnt = 2;
//	  xprint(" args ");
//	  xprint_xchar(arguments);
	  while (arguments-- > 0) {
		  wdata[bytecnt++] = *list;
//		  xprint(" ");
//		  xprint_xchar(*list);
		  list++;
	  }
	  transaction.writeCount = bytecnt;
//	  xprint(" i2cout ");
//	  xprint_char(bytecnt);
//	  xprint("\n");
	  I2C_transfer(OLEDHandle, &transaction);
	  if(transaction.status != 0) {
	        return(transaction.status);
	  }
	  if(cmdbyte == 0xdb)
		  delay(5);


//	    status = SSD1306_Send_Command (*list++);
//    if (SSD1306_SUCCESS != status) {
//      return status;
//    }
    // Arguments
    // -----------------------------------------------------------------------------------
//    arguments = pgm_read_byte (list++);
//    arguments = list++;
//    while (arguments--) {
//        status = SSD1306_Send_Command (pgm_read_byte(list++));  // argument
//        status = SSD1306_Send_Command (*list++);  // argument
//      if (SSD1306_SUCCESS != status) {
//        return status;
//      }
    }
    xprint("\n end of list\n");
//  }
  // TWI: Stop
  // -------------------------------------------------------------------------------------
//  TWI_Stop (); GW

  return SSD1306_SUCCESS;
}

/**
 * @brief   SSD1306 Send Start and SLAW request
 *
 * @param   uint8_t
 *
 * @return  uint8_t
 */
uint8_t SSD1306_Send_StartAndSLAW (uint8_t address)
{
//  uint8_t status = INIT_STATUS;

  // TWI: start
  // -------------------------------------------------------------------------------------
/*  status = TWI_MT_Start ();
  if (SSD1306_SUCCESS != status) {
    return status;
  }
  // TWI: send SLAW
  // -------------------------------------------------------------------------------------
  status = TWI_MT_Send_SLAW (address);
  if (SSD1306_SUCCESS != status) {
    return status;
  } */

  return SSD1306_SUCCESS;
}

/**
 * @brief   SSD1306 Send command
 *
 * @param   uint8_t command
 *
 * @return  uint8_t
 */
uint8_t SSD1306_Send_Command (uint8_t command)
{
  uint8_t status = INIT_STATUS;

  transaction.writeBuf = &wdata;
  transaction.readCount = 0; //sizeof(rdata);
  transaction.writeCount = 2;
  wdata[0] = 0;
  wdata[1] = command;
//    Expander_Write(0);
  I2C_transfer(OLEDHandle, &transaction);
//  if(transaction.status != 0) {
      return(transaction.status);
//  }
}

/**
 * +------------------------------------------------------------------------------------+
 * |== PUBLIC FUNCTIONS ================================================================|
 * +------------------------------------------------------------------------------------+
 */
int SSD1306_Begin(void) {
    // One-time init of I2C driver
    I2C_init();
    // initialize optional I2C bus parameters
    I2C_Params_init(&OLEDparams);
    OLEDparams.bitRate = I2C_400kHz;
    // Open I2C bus for usage
    OLEDHandle = I2C_open(CONFIG_I2C_0, &OLEDparams);
    if (OLEDHandle == NULL) {
        // Error opening I2C
        while (1) {}
    }
    // Initialize target address of transaction
    transaction.targetAddress = OLED_ADDR;
    // Read from I2C target device
    transaction.readBuf = &rdata;
    transaction.writeBuf = &wdata;
    transaction.readCount = 0; //sizeof(rdata);
    transaction.writeCount = 1;
    wdata[0] = 0;
	I2C_transfer(OLEDHandle, &transaction);
//    Expander_Write(0);
    if(transaction.status != 0) {
        return(transaction.status);
    }
    SSD1306_Init(0x3c);
/*    LCD_Cmd(3);
    LCD_Cmd(3);
    LCD_Cmd(3);
    LCD_Cmd(LCD_RETURN_HOME);
    LCD_Cmd(0x20 | (LCD_TYPE << 2));
    LCD_Cmd(LCD_TURN_ON);
    LCD_Cmd(LCD_CLEAR);
    LCD_Cmd(LCD_ENTRY_MODE_SET | LCD_RETURN_HOME); */
    return(transaction.status);
}

/**
 * @brief   SSD1306 Normal colors
 *
 * @param   uint8_t address
 *
 * @return  uint8_t
 */
uint8_t SSD1306_NormalScreen (uint8_t address)
{
  uint8_t status = INIT_STATUS;

  // TWI: start & SLAW
  // -------------------------------------------------------------------------------------
  status = SSD1306_Send_StartAndSLAW (address);
  if (SSD1306_SUCCESS != status) {
    return status;
  }
  // send command
  // -------------------------------------------------------------------------------------   
  status = SSD1306_Send_Command (SSD1306_DIS_NORMAL);
  if (SSD1306_SUCCESS != status) {
    return status;
  }
  // TWI: Stop
  // -------------------------------------------------------------------------------------
  //TWI_Stop ();

  return SSD1306_SUCCESS;
}

/**
 * @brief   SSD1306 Inverse colors
 *
 * @param   uint8_t address
 *
 * @return  uint8_t
 */
uint8_t SSD1306_InverseScreen (uint8_t address)
{
  uint8_t status = INIT_STATUS;

  // TWI: start & SLAW
  // -------------------------------------------------------------------------------------
//  status = SSD1306_Send_StartAndSLAW (address);
//  if (SSD1306_SUCCESS != status) {
//    return status;
//  }
  // send command
  // -------------------------------------------------------------------------------------   
  status = SSD1306_Send_Command (SSD1306_DIS_INVERSE);
  if (SSD1306_SUCCESS != status) {
    return status;
  }
  // TWI: Stop
  // -------------------------------------------------------------------------------------
  //TWI_Stop ();

  return SSD1306_SUCCESS;
}

/**
 * @brief   SSD1306 Update screen
 *
 * @param   uint8_t address
 *
 * @return  uint8_t
 */
uint8_t SSD1306_UpdateScreen (uint8_t address)
{
  uint8_t status = INIT_STATUS;
  uint16_t i = 0;

  // -------------------------------------------------------------------------------------
  //status = SSD1306_Send_StartAndSLAW (address);
  //if (SSD1306_SUCCESS != status) {
//    return status;
//  }
  // control byte data stream
  // -------------------------------------------------------------------------------------   
#define CHUNK 32
  int cursent = 0;
  while(cursent < CACHE_SIZE_MEM) {
    transaction.writeBuf = &wdata;
    transaction.readCount = 0; //sizeof(rdata);
    transaction.writeCount = 1;
//    wdata[0] = 0;
    wdata[0] = SSD1306_DATA_STREAM;
//    Expander_Write(0);
//    I2C_transfer(OLEDHandle, &transaction);
//    if(transaction.status != 0) {
//      return(transaction.status);
//    }
// status = TWI_MT_Send_Data (SSD1306_DATA_STREAM);
//  if (SSD1306_SUCCESS != status) {
//    return status;
//  }
  //  send cache memory lcd
  // -------------------------------------------------------------------------------------
//  transaction.writeBuf = &cacheMemLcd;
    transaction.readCount = 0; //sizeof(rdata);
//  transaction.writeCount = CACHE_SIZE_MEM;
//    Expander_Write(0);
//	  wdata[0] = 0x0;
//	  wdata[1] = 0x40;
	  memcpy(&wdata[1],&cacheMemLcd[cursent],CHUNK);	// copy a message packet
      transaction.writeBuf = &wdata;
      transaction.writeCount = CHUNK+1;
      I2C_transfer(OLEDHandle, &transaction);
      if(transaction.status != 0) {
        return(transaction.status);
      }
      cursent = cursent + CHUNK;
  }
//  xprint("Updated ");
//  xprint_int(cursent);
//  xprint("\n");
//  }
  // stop TWI
  // -------------------------------------------------------------------------------------
//  TWI_Stop ();

  return SSD1306_SUCCESS;
}

/**
 * @brief   SSD1306 Clear screen
 *
 * @param   void
 *
 * @return  void
 */
void SSD1306_ClearScreen (void)
{
  memset (cacheMemLcd, 0x00, CACHE_SIZE_MEM);                     // null cache memory lcd
}

/**
 * @brief   SSD1306 Set position
 *
 * @param   uint8_t column -> 0 ... 127 
 * @param   uint8_t page -> 0 ... 7 or 3 
 *
 * @return  void
 */
int SSD_GetPosition(void) {
	return _counter;
}
void SSD1306_SetPosition (uint8_t x, uint8_t y) 
{
  _counter = x + (y << 7);                                        // update counter
}

/**
 * @brief   SSD1306 Update text poisition - this ensure that character will not be divided at the end of row, 
 *          the whole character will be depicted on the new row
 *
 * @param   void
 *
 * @return  uint8_t
 */
uint8_t SSD1306_UpdatePosition (void) 
{
  uint8_t y = _counter >> 7;                                      // y / 8
  uint8_t x = _counter - (y << 7);                                // y % 8
  uint8_t x_new = x + CHARS_COLS_LENGTH + 1;                      // x + character length + 1
  
  if (x_new > END_COLUMN_ADDR) {                                  // check position
    if (y > END_PAGE_ADDR) {                                      // if more than allowable number of pages
      return SSD1306_ERROR;                                       // return out of range
    } else if (y < (END_PAGE_ADDR-1)) {                           // if x reach the end but page in range
      _counter = ((++y) << 7);                                    // update
    }
  }
 
  return SSD1306_SUCCESS;
}

/**
 * @brief   SSD1306 Draw character
 *
 * @param   char character
 *
 * @return  uint8_t
 */
uint8_t SSD1306_DrawChar (char character)
{
  uint8_t i = 0;
  char outbuf[16];

  if (SSD1306_UpdatePosition () == SSD1306_ERROR) {
    return SSD1306_ERROR;
  }
  while (i < CHARS_COLS_LENGTH) {
//	    sprintf(outbuf,"%08x\n",*(&FONTS[character-32][i]));
//	    xprint(outbuf);

    cacheMemLcd[_counter++] = *(&FONTS[character-32][i++]);
  }
  _counter++;

  return SSD1306_SUCCESS;
}
void SSD1306_DrawBlock (char shape) {
//	int i = 0;
	cacheMemLcd[_counter++] = 0xbd;
	cacheMemLcd[_counter++] = 0xbd;
	cacheMemLcd[_counter++] = 0xbd;
	cacheMemLcd[_counter++] = 0xbd;
	cacheMemLcd[_counter++] = 0xbd;
	cacheMemLcd[_counter++] = 0xbd;
	cacheMemLcd[_counter++] = 0xbd;
	cacheMemLcd[_counter++] = 0xbd;
}
void SSD1306_ClearBlock (char shape) {
//	int i = 0;
	cacheMemLcd[_counter++] = 0x0;
	cacheMemLcd[_counter++] = 0x0;
	cacheMemLcd[_counter++] = 0x0;
	cacheMemLcd[_counter++] = 0x0;
	cacheMemLcd[_counter++] = 0x0;
	cacheMemLcd[_counter++] = 0x0;
	cacheMemLcd[_counter++] = 0x0;
	cacheMemLcd[_counter++] = 0x0;
}

/**
 * @brief   SSD1306 Draw String
 *
 * @param   char * string
 *
 * @return  void
 */
void SSD1306_DrawString (char *str)
{
  int i = 0;
  while (str[i] != '\0') {
    SSD1306_DrawChar (str[i++]);
  }
}

/**
 * @brief   Draw pixel
 *
 * @param   uint8_t x -> 0 ... MAX_X
 * @param   uint8_t y -> 0 ... MAX_Y
 *
 * @return  uint8_t
 */
uint8_t SSD1306_DrawPixel (uint8_t x, uint8_t y)
{
  uint8_t page = 0;
  uint8_t pixel = 0;
  
  if ((x > MAX_X) || (y > MAX_Y)) {                               // if out of range
    return SSD1306_ERROR;                                         // out of range
  }
  page = y >> 3;                                                  // find page (y / 8)
  pixel = 1 << (y - (page << 3));                                 // which pixel (y % 8)
  _counter = x + (page << 7);                                     // update counter
  cacheMemLcd[_counter++] |= pixel;                               // save pixel

  return SSD1306_SUCCESS;
}

/**
 * @brief   Draw line by Bresenham algoritm
 *  
 * @param   uint8_t x start position / 0 <= cols <= MAX_X-1
 * @param   uint8_t x end position   / 0 <= cols <= MAX_X-1
 * @param   uint8_t y start position / 0 <= rows <= MAX_Y-1 
 * @param   uint8_t y end position   / 0 <= rows <= MAX_Y-1
 *
 * @return  uint8_t
 */
uint8_t SSD1306_DrawLine (uint8_t x1, uint8_t x2, uint8_t y1, uint8_t y2)
{
  int16_t D;                                                      // determinant
  int16_t delta_x, delta_y;                                       // deltas
  int16_t trace_x = 1, trace_y = 1;                               // steps

  delta_x = x2 - x1;                                              // delta x
  delta_y = y2 - y1;                                              // delta y
  
  if (delta_x < 0) {                                              // check if x2 > x1
    delta_x = -delta_x;                                           // negate delta x
    trace_x = -trace_x;                                           // negate step x
  }
  
  if (delta_y < 0) {                                              // check if y2 > y1
    delta_y = -delta_y;                                           // negate detla y
    trace_y = -trace_y;                                           // negate step y
  }

  // Bresenham condition for m < 1 (dy < dx)
  // -------------------------------------------------------------------------------------
  if (delta_y < delta_x) {
    D = (delta_y << 1) - delta_x;                                 // calculate determinant
    SSD1306_DrawPixel (x1, y1);                                   // draw first pixel
    while (x1 != x2) {                                            // check if x1 equal x2
      x1 += trace_x;                                              // update x1
      if (D >= 0) {                                               // check if determinant is positive
        y1 += trace_y;                                            // update y1
        D -= 2*delta_x;                                           // update determinant
      }
      D += 2*delta_y;                                             // update deteminant
      SSD1306_DrawPixel (x1, y1);                                 // draw next pixel
    }
  // for m > 1 (dy > dx)    
  // -------------------------------------------------------------------------------------
  } else {
    D = delta_y - (delta_x << 1);                                 // calculate determinant
    SSD1306_DrawPixel (x1, y1);                                   // draw first pixel
    while (y1 != y2) {                                            // check if y2 equal y1
      y1 += trace_y;                                              // update y1
      if (D <= 0) {                                               // check if determinant is positive
        x1 += trace_x;                                            // update y1
        D += 2*delta_y;                                           // update determinant
      }
      D -= 2*delta_x;                                             // update deteminant
      SSD1306_DrawPixel (x1, y1);                                 // draw next pixel
    }
  }

  return SSD1306_SUCCESS;
}
