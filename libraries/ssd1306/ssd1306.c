/*
 * ssd1306.c
 *
 *  Created on: 20.08.2016
 *      Author: Arkadiusz Pytlik
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "ssd1306.h"

/* SSD1306 commands. */
#define SSD1306_SETLOWCOLUMN 							0x00
#define SSD1306_EXTERNALVCC 							0x01
#define SSD1306_SWITCHCAPVCC 							0x02
#define SSD1306_SETHIGHCOLUMN 							0x10
#define SSD1306_MEMORYMODE 								0x20
#define SSD1306_COLUMNADDR 								0x21
#define SSD1306_PAGEADDR   								0x22
#define SSD1306_RIGHT_HORIZONTAL_SCROLL 				0x26
#define SSD1306_LEFT_HORIZONTAL_SCROLL 					0x27
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 	0x29
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 	0x2A
#define SSD1306_DEACTIVATE_SCROLL 						0x2E
#define SSD1306_ACTIVATE_SCROLL 						0x2F
#define SSD1306_SETSTARTLINE 							0x40
#define SSD1306_SETCONTRAST 							0x81
#define SSD1306_CHARGEPUMP 								0x8D
#define SSD1306_SEGREMAP 								0xA0
#define SSD1306_SET_VERTICAL_SCROLL_AREA 				0xA3
#define SSD1306_DISPLAYALLON_RESUME 					0xA4
#define SSD1306_DISPLAYALLON 							0xA5
#define SSD1306_NORMALDISPLAY 							0xA6
#define SSD1306_INVERTDISPLAY 							0xA7
#define SSD1306_SETMULTIPLEX 							0xA8
#define SSD1306_DISPLAYOFF 								0xAE
#define SSD1306_DISPLAYON 								0xAF
#define SSD1306_SETPAGESTARTADDRESS						0xB0
#define SSD1306_COMSCANINC 								0xC0
#define SSD1306_COMSCANDEC 								0xC8
#define SSD1306_SETDISPLAYOFFSET 						0xD3
#define SSD1306_SETDISPLAYCLOCKDIV 						0xD5
#define SSD1306_SETPRECHARGE 							0xD9
#define SSD1306_SETCOMPINS 								0xDA
#define SSD1306_SETVCOMDESELECT							0xDB

static uint8_t Init_Table[]=
{
	SSD1306_DISPLAYOFF,
	SSD1306_SETLOWCOLUMN,
	SSD1306_SETHIGHCOLUMN,
	SSD1306_SETPAGESTARTADDRESS,
	SSD1306_SETSTARTLINE,
	SSD1306_SEGREMAP | 0x01,
	SSD1306_SETCOMPINS,
	0x12,	/* 0x02 for 32rows. Set com pins data. xxx */
	SSD1306_SETDISPLAYOFFSET,
	0x00,	/* Set display offset data. No offset. */
	SSD1306_COMSCANDEC,
	SSD1306_NORMALDISPLAY,
	SSD1306_DISPLAYALLON_RESUME,
	SSD1306_SETCONTRAST,
	0x00,	/* Set contrast data. */
	SSD1306_MEMORYMODE,
	0x00,	/* Memory addressing mode data. Horizontal addressing. */
	SSD1306_SETMULTIPLEX,
	0x3F,	/* 0x1F for 32 rows. Set MUX ratio data. 1/32 duty cycle. xxx */
	SSD1306_SETPRECHARGE,
	0xF1,	/* Set pre-charge period data. */
	SSD1306_SETVCOMDESELECT,
	0x40,	/* Set V com deselect data. */
	SSD1306_CHARGEPUMP,
	0x14,	/* Charge pump setting data. */
	SSD1306_DISPLAYON,
};

static FONT_INFO CurrentFont;


#if defined(USE_DOTF_FONT)
static uint16_t CP1250_to_UTF8(uint8_t in)
{
	uint16_t out = in;
	/* remap */

	if(in == 140) {out = 346;} /* Œ */
	else if(in == 143) {out = 377;} /*  */
	else if(in == 156) {out = 347;} /* œ */
	else if(in == 159) {out = 378;} /* Ÿ */
	else if(in == 163) {out = 321;} /* £ */
	else if(in == 165) {out = 260;} /* ¥ */
	else if(in == 175) {out = 379;} /* ¯ */
	else if(in == 179) {out = 322;} /* ³ */
	else if(in == 185) {out = 261;} /* ¹ */
	else if(in == 191) {out = 380;} /* ¿ */
	else if(in == 198) {out = 262;} /* Æ */
	else if(in == 202) {out = 280;} /* Ê */
	else if(in == 209) {out = 323;} /* Ñ */
	else if(in == 211) {out = 211;} /* Ó */
	else if(in == 230) {out = 263;} /* æ */
	else if(in == 234) {out = 281;} /* ê */
	else if(in == 241) {out = 324;} /* ñ */
	else if(in == 243) {out = 243;} /* ó */

	return out;
}
#endif /* USE_DOTF_FONT */


void SSD1306_UpdateDisplay(SSD1306_InitType *h)
{
	uint8_t Command[] =
	{
		SSD1306_SETLOWCOLUMN,
		SSD1306_SETHIGHCOLUMN,
		SSD1306_SETPAGESTARTADDRESS
	};

	if(h->DisplayAddress == 0 || h->WriteFunction == 0)
	{
		return;
	}

	/* Set the initial coordinates. */
	h->WriteFunction(h->DisplayAddress, 0x80,
			Command, sizeof(Command) );

	/* Send buffer to the display */
	h->WriteFunction(h->DisplayAddress, 0x40,
			h->FrameBuffer, h->BufferLength );
}

void SSD1306_Goto(SSD1306_InitType *h, int x, int y)
{
	h->x = x;
	h->y = y;
}

void SSD1306_SetColor(SSD1306_InitType *h, uint8_t c)
{
	h->Color = c;
}

void SSD1306_DrawPixel(SSD1306_InitType *h, int x, int y)
{
	if(x > (h->Width - 1) || x < 0) return;
	if(y > (h->Height - 1) || y < 0) return;

	uint16_t index = ((y / 8) * h->Width) + x;
	uint8_t offset = y % 8;

	h->FrameBuffer[index] = (h->Color)?
			h->FrameBuffer[index] | (1 << offset) :
			h->FrameBuffer[index] & ~(1 << offset);
}

void SSD1306_DrawLine(SSD1306_InitType *h, uint8_t X1,
		uint8_t Y1, uint8_t X2, uint8_t Y2)
{
	int CurrentX, CurrentY, Xinc, Yinc, Dx, Dy, TwoDx, TwoDy,
			TwoDxAccumulatedError, TwoDyAccumulatedError;

	//Bresenham algorithm
	Dx = (X2 - X1);
	Dy = (Y2 - Y1);

	TwoDx = Dx + Dx;
	TwoDy = Dy + Dy;

	CurrentX = X1;
	CurrentY = Y1;

	Xinc = 1;
	Yinc = 1;

	if (Dx < 0)
	{
		Xinc = -1;
		Dx = -Dx;
		TwoDx = -TwoDx;
	}

	if (Dy < 0)
	{
		Yinc = -1;
		Dy = -Dy;
		TwoDy = -TwoDy;
	}

	SSD1306_DrawPixel(h, X1, Y1);

	if ((Dx != 0) || (Dy != 0))
	{
		if (Dy <= Dx)
		{
			TwoDxAccumulatedError = 0;
			do
			{
				CurrentX += Xinc;
				TwoDxAccumulatedError += TwoDy;
				if (TwoDxAccumulatedError > Dx)
				{
					CurrentY += Yinc;
					TwoDxAccumulatedError -= TwoDx;
				}
				SSD1306_DrawPixel(h, CurrentX, CurrentY);

			} while (CurrentX != X2);
		}
		else
		{
			TwoDyAccumulatedError = 0;
			do
			{
				CurrentY += Yinc;
				TwoDyAccumulatedError += TwoDx;
				if (TwoDyAccumulatedError > Dy)
				{
					CurrentX += Xinc;
					TwoDyAccumulatedError -= TwoDy;
				}
				SSD1306_DrawPixel(h, CurrentX, CurrentY);

			} while (CurrentY != Y2);
		}
	}
}

void SSD1306_DrawEllipse(SSD1306_InitType *h, int xm, int ym, int a, int b)
{
   int x = -a;
   int y = 0;		/* II. quadrant from bottom left to top right */

   long e2 = (long)b * b;
   long err = (long)x * (2 * e2 + x) + e2;		/* error of 1.step */

   do
   {
	   SSD1306_DrawPixel(h, xm - x, ym + y);		/*   I. Quadrant */
	   SSD1306_DrawPixel(h, xm + x, ym + y);		/*  II. Quadrant */
	   SSD1306_DrawPixel(h, xm + x, ym  -y);		/* III. Quadrant */
	   SSD1306_DrawPixel(h, xm - x, ym - y);		/*  IV. Quadrant */

	   e2 = 2 * err;

       if (e2 >= (x * 2 + 1) * (long)b * b)		/* e_xy+e_x > 0 */
       {
    	   err += (++x * 2 + 1) * (long)b * b;
       }

       if (e2 <= (y * 2 + 1) * (long)a * a)		/* e_xy+e_y < 0 */
       {
    	   err += (++y * 2 + 1) * (long)a * a;
       }

   } while (x <= 0);

   while (y++ < b)
   {
	   /* too early stop of flat ellipses a=1, */
	   SSD1306_DrawPixel(h, xm, ym + y);			/* -> finish tip of ellipse */
	   SSD1306_DrawPixel(h, xm, ym - y);
   }
}

void SSD1306_DrawBattery(SSD1306_InitType *h, uint8_t x, uint8_t y, uint8_t value)
{
    uint8_t i = 0;
	SSD1306_DrawLine(h, x, y, x, y + 8);
	SSD1306_DrawLine(h, x, y, x + 11, y);
	SSD1306_DrawLine(h, x, y + 8, x + 11, y + 8);
	SSD1306_DrawLine(h, x + 12, y , x + 12, y + 8);
	SSD1306_DrawLine(h, x + 13, y + 2, x + 13, y + 6);
	SSD1306_DrawLine(h, x + 14, y + 2, x + 14, y + 6);

	if(value > 100)
	{
		value = 100;
	}

	value /= 10;

	for(i = 0; i < 10; ++i)
	{
		if(value != 0)
		{
			SSD1306_SetColor(h, 1);
			value--;
		}
		else
		{
			SSD1306_SetColor(h, 0);
		}
		SSD1306_DrawLine(h, x + i + 1, y + 1, x + i + 1, y + 7);
	}

	SSD1306_SetColor(h, 1);
}

void SSD1306_Cls(SSD1306_InitType *h)
{
	memset(h->FrameBuffer, 0, h->BufferLength);
}

void SSD1306_Init(SSD1306_InitType *h)
{
	if(h->DisplayAddress == 0 || h->WriteFunction == 0 || h->DelayFunction == 0)
	{
		return;
	}

	if(h->Height == 64)
	{
		Init_Table[7] = 0x12;
		Init_Table[18] = 0x3F;
	}
	else if(h->Height == 32)
	{
		Init_Table[7] = 0x02;
		Init_Table[18] = 0x1F;
	}

	//h->DelayFunction(150);
	h->WriteFunction(h->DisplayAddress, 0x80, Init_Table, sizeof(Init_Table) );
	h->DelayFunction(150);

	SSD1306_Cls(h);
	SSD1306_UpdateDisplay(h);
}


void SSD1306_SetFont(SSD1306_InitType *h, const FONT_INFO *font)
{
	if(h == 0 || font == 0)
	{
		return;
	}

	memcpy(&CurrentFont, font, sizeof(FONT_INFO));//pgmspace.h
}

void SSD1306_Putc(SSD1306_InitType *h, char c)
{
	uint8_t width;
	uint8_t height;
	uint8_t charidx;
	uint8_t bits;
	uint8_t data;
	uint8_t color = h->Color;
	uint16_t bitmapidx;
	int x = h->x, y = h->y;

	/* character height (in pixels) */
	height = CurrentFont.heightPixels;

	if(c == ' ')
	{
		SSD1306_SetColor(h, 0);
		while(height--)
		{
			width = CurrentFont.spacePixels;
			while(width--)
			{
				SSD1306_DrawPixel(h, x++, y);
			}

			x = h->x;
			y++;
		}
		SSD1306_Goto(h, h->x + CurrentFont.spacePixels, h->y);
	}
	else if(c > ' ')
	{
#if defined(USE_DOTF_FONT)

		/* szukam polskich znaków w kodowaniu CP1250 */
		if(c > 139 && c < 244)
		{
			charidx = CP1250_to_UTF8(c) - CurrentFont.startChar;
		}
		else
		{
			charidx = c - CurrentFont.startChar;
		}

#elif defined(USE_PIXELF_FONT)
		charidx = c - CurrentFont.startChar;
#endif

		bitmapidx =  CurrentFont.charInfo[charidx].offset;

		/* bitmap drawing */
		while(height--)
		{
			width =  CurrentFont.charInfo[charidx].widthBits;
			while(width)
			{
				data =CurrentFont.data[ (bitmapidx)++ ];

				if(width < 8)
				{
					bits = width;
					width = 0;
				}
				else
				{
					width -= 8;
					bits = 8;
				}
				while(bits--)
				{
					if(  ( (data & 0x80) != 0)  &&  (color != 0)  )
					{
						SSD1306_SetColor(h, 1);
					}
					else
					{
						SSD1306_SetColor(h, 0);
					}
					SSD1306_DrawPixel(h, x++, y);
					data <<= 1;
				}
			}
			x = h->x;
			y++;
		}

		x = h->x + CurrentFont.charInfo[charidx].widthBits  ;
		y = h->y;

		SSD1306_Goto(h, x, y);

		/* interspace drawing */

		/* character height (in pixels) */
		height = CurrentFont.heightPixels;

		/* set color to background */
		SSD1306_SetColor(h, 0);

		while(height--)
		{

#if defined(USE_PIXELF_FONT)
			width = CurrentFont.interspacePixels;
#elif defined(USE_DOTF_FONT)
			width = FONT_INTERSPACE;
#endif /* USE_PIXELF || USE_DOTF */

			while(width--)
			{
				SSD1306_DrawPixel(h, x++, y);
			}

			x = h->x;
			y++;
		}
#if defined(USE_PIXELF_FONT)
		SSD1306_Goto(h, h->x + CurrentFont.interspacePixels, coord.y);
#elif defined(USE_DOTF_FONT)
		SSD1306_Goto(h, h->x + FONT_INTERSPACE, h->y);
#endif /* USE_PIXELF || USE_DOTF */
	}
	SSD1306_SetColor(h, color);
}

void SSD1306_Puts(SSD1306_InitType *h, char *s)
{
	while(*s)
	{
		SSD1306_Putc(h, *s++);
	}
}

void SSD1306_Puts_P(SSD1306_InitType *h, const char *s)
{
	char c;

	while((c = *s++))
	{
		SSD1306_Putc(h, c);
	}
}



