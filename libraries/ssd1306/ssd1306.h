/*
 * ssd1306.h
 *
 *  Created on: 20.08.2016
 *      Author: Arkadiusz Pytlik
 */

#ifndef SRC_SSD1306_SSD1306_H_
#define SRC_SSD1306_SSD1306_H_

#include <stdlib.h>

#define USE_DOTF_FONT
//#define USE_PIXELF_FONT

#if defined(USE_DOTF_FONT)
	#define FONT_INTERSPACE	6
#endif /* USE_DOTF_FONT */



/* Single character's display information typedef. */
typedef struct
{
	/* Width, in bits (or pixels), of the character. */
	const uint8_t widthBits;

	/* Offset of the character's bitmap into the the FONT_INFO's data array. */
	const uint16_t offset;

} FONT_CHAR_INFO;

/* Font configuration typedef. */
typedef struct
{
	/* Height, in pixels, of the font's characters. */
	uint8_t heightPixels;

#if defined(USE_PIXELF_FONT)
	/* The first defined character. */
	uint8_t startChar;
	/* Number of pixels of interspace between characters. */
	uint8_t interspacePixels;
#elif defined(USE_DOTF_FONT)
	/* The first defined character. */
	uint16_t startChar;
	/* The last defined character */
	uint16_t endChar;
#endif

	/* Number of pixels of space character. */
	uint8_t spacePixels;

	/* Pointer to array of char descriptors. */
	const FONT_CHAR_INFO *charInfo;

	/* Pointer to character's bitmaps. */
	const uint8_t *data;

#if defined(USE_PIXELF_FONT)
	/* Font filename saved on SD card. */
    char *FontFileName;
#endif
} FONT_INFO;



/* I2C write function callback typedef. */
typedef int (SSD1306_CommType)(uint8_t SlaveAddr, uint8_t WordAddr,
		uint8_t *Buffer, size_t BufferLength);




/* Global display's handle typedef. */
typedef struct
{
	/* I2C address (0x78 or 0x7A). Defined by user */
	uint8_t DisplayAddress;

	/* Pointer to the I2C low level function. Defined by user. */
	SSD1306_CommType *WriteFunction;

	/* Pointer to the delay_ms function. Defined by user. */
	void (*DelayFunction)(uint16_t ms);

	/* Display height in pixels. Defined by user. */
	uint8_t Height;

	/* Display width in pixel. Defined by user. */
	uint8_t Width;

	/* Pointer to the frame buffer. Buffer length must be exactly
	 * (display height * display width / 8). Defined by user. */
	uint8_t *FrameBuffer;

	/* Buffer length in bytes. Defined by user. */
	size_t BufferLength;

	/* X coordinate. Managed by driver. */
	int x;

	/* Y coordinate. Managed by driver. */
	int y;

	/* Current color. 0 - background color, other than 0 - pixel will be set.
	 * Managed by driver. */
	uint8_t Color;
}SSD1306_InitType;


void SSD1306_Init(SSD1306_InitType *h);
void SSD1306_UpdateDisplay(SSD1306_InitType *h);
void SSD1306_Cls(SSD1306_InitType *h);

void SSD1306_Goto(SSD1306_InitType *h, int x, int y);
void SSD1306_SetColor(SSD1306_InitType *h, uint8_t c);
void SSD1306_DrawPixel(SSD1306_InitType *h, int x, int y);
void SSD1306_DrawLine(SSD1306_InitType *h, uint8_t X1, uint8_t Y1,
		uint8_t X2, uint8_t Y2);
void SSD1306_DrawEllipse(SSD1306_InitType *h, int xm, int ym, int a, int b);
void SSD1306_DrawBattery(SSD1306_InitType *h, uint8_t x, uint8_t y,
		uint8_t value);

void SSD1306_SetFont(SSD1306_InitType *h, const FONT_INFO *font);
void SSD1306_Putc(SSD1306_InitType *h, char c);
void SSD1306_Puts(SSD1306_InitType *h, char *s);
void SSD1306_Puts_P(SSD1306_InitType *h, const char *s);

#endif /* SRC_SSD1306_SSD1306_H_ */
