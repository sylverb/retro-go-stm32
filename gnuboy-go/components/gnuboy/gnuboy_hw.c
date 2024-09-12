#include <string.h>

#include "defs.h"
#include "cpu.h"
#include "hw.h"
#include "regs.h"
#include "lcd.h"
#include "mem.h"


struct hw hw;


/*
 * hw_interrupt changes the virtual interrupt line(s) defined by i
 * The interrupt fires (added to R_IF) when the line transitions from 0 to 1.
 * It does not refire if the line was already high.
 */
void hw_interrupt(byte i, int level)
{
	if (level == 0)
	{
		hw.ilines &= ~i;
	}
	else if ((hw.ilines & i) == 0)
	{
		hw.ilines |= i;
		R_IF |= i; // Fire!

		if ((R_IE & i) != 0)
		{
			// Wake up the CPU when an enabled interrupt occurs
			// IME doesn't matter at this point, only IE
			cpu.halt = 0;
		}
	}
}


/*
 * hw_dma performs plain old memory-to-oam dma, the original dmg
 * dma. Although on the hardware it takes a good deal of time, the cpu
 * continues running during this mode of dma, so no special tricks to
 * stall the cpu are necessary.
 */
void IRAM_ATTR hw_dma(byte b)
{
	addr a = ((addr)b) << 8;
	for (int i = 0; i < 160; i++, a++)
		lcd.oam.mem[i] = readb(a);
}


void IRAM_ATTR hw_hdma_cmd(byte c)
{
	/* Begin or cancel HDMA */
	if ((hw.hdma|c) & 0x80)
	{
		hw.hdma = c;
		R_HDMA5 = c & 0x7f;
		return;
	}

	/* Perform GDMA */
	int sa = ((addr)R_HDMA1 << 8) | (R_HDMA2&0xf0);
	int da = 0x8000 | ((int)(R_HDMA3&0x1f) << 8) | (R_HDMA4&0xf0);
	int cnt = ((int)c)+1;
	/* FIXME - this should use cpu time! */
	/*cpu_timers(102 * cnt);*/
	cnt <<= 4;
	while (cnt--)
		writeb(da++, readb(sa++));
	R_HDMA1 = sa >> 8;
	R_HDMA2 = sa & 0xF0;
	R_HDMA3 = 0x1F & (da >> 8);
	R_HDMA4 = da & 0xF0;
	R_HDMA5 = 0xFF;
}


void IRAM_ATTR hw_hdma()
{
	int sa = ((addr)R_HDMA1 << 8) | (R_HDMA2&0xf0);
	int da = 0x8000 | ((int)(R_HDMA3&0x1f) << 8) | (R_HDMA4&0xf0);
	int cnt = 16;

	while (cnt--)
		writeb(da++, readb(sa++));
	R_HDMA1 = sa >> 8;
	R_HDMA2 = sa & 0xF0;
	R_HDMA3 = 0x1F & (da >> 8);
	R_HDMA4 = da & 0xF0;
	R_HDMA5--;
	hw.hdma--;
}


/*
 * pad_refresh updates the P1 register from the pad states, generating
 * the appropriate interrupts (by quickly raising and lowering the
 * interrupt line) if a transition has been made.
 */
void IRAM_ATTR pad_refresh()
{
	byte oldp1 = R_P1;
	R_P1 &= 0x30;
	R_P1 |= 0xc0;
	if (!(R_P1 & 0x10)) R_P1 |= (hw.pad & 0x0F);
	if (!(R_P1 & 0x20)) R_P1 |= (hw.pad >> 4);
	R_P1 ^= 0x0F;
	if (oldp1 & ~R_P1 & 0x0F)
	{
		hw_interrupt(IF_PAD, 1);
		hw_interrupt(IF_PAD, 0);
	}
}


/*
 * pad_set updates the state of one or more buttons on the pad and calls
 * pad_refresh() to fire an interrupt if the pad changed.
 */
void IRAM_ATTR pad_set(byte btn, int set)
{
	int new_pad = hw.pad;

	if (set)
		new_pad |= btn;
	else
		new_pad &= ~btn;

	if (hw.pad != new_pad)
	{
		hw.pad = new_pad;
		pad_refresh();
	}
}

void hw_reset()
{
	hw.ilines = 0;
	hw.serial = 0;
	hw.pad = 0;

	memset(ram.hi, 0, sizeof ram.hi);

	R_P1 = 0xFF;
	R_LCDC = 0x91;
	R_BGP = 0xFC;
	R_OBP0 = 0xFF;
	R_OBP1 = 0xFF;
	R_SVBK = 0xF9;
	R_HDMA5 = 0xFF;
	R_VBK = 0xFE;
}
