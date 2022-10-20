#include <stdio.h>
#include <time.h>

#include "defs.h"
#include "mem.h"
#include "rtc.h"
#include "rg_rtc.h"

struct rtc rtc;

void rtc_latch(byte b)
{
	if ((rtc.latch ^ b) & b & 1)
	{
		if (!(rtc.flags & 0x40)) // rtc stop
		{
			un32 time = (un32)GW_GetUnixTime() - rtc.epoch;

			rtc.s = time % 60;
			rtc.m = (time / 60) % 60;
			rtc.h = (time / 3600) % 24;
			rtc.d = (time / 86400);

			if (rtc.d >= 512)
				rtc.flags |= 0x80;
		}

		rtc.regs[0] = rtc.s;
		rtc.regs[1] = rtc.m;
		rtc.regs[2] = rtc.h;
		rtc.regs[3] = rtc.d;
		rtc.regs[4] = (rtc.flags & 0xfe) | ((rtc.d>>8)&1);
		rtc.regs[5] = 0xff;
		rtc.regs[6] = 0xff;
		rtc.regs[7] = 0xff;
	}
	rtc.latch = b;
}

void rtc_write(byte b)
{
	/* printf("write %02X: %02X (%d)\n", rtc.sel, b, b); */
	switch (rtc.sel & 0xf)
	{
	case 0x8: // Seconds
		rtc.regs[0] = b;
		rtc.s = b % 60;
		break;
	case 0x9: // Minutes
		rtc.regs[1] = b;
		rtc.m = b % 60;
		break;
	case 0xA: // Hours
		rtc.regs[2] = b;
		rtc.h = b % 24;
		break;
	case 0xB: // Days (lower 8 bits)
		rtc.regs[3] = b;
		rtc.d = ((rtc.d & 0x100) | b);
		break;
	case 0xC: // Flags (days upper 1 bit, carry, stop)
		rtc.regs[4] = b;
		rtc.flags = b;
		rtc.d = ((rtc.d & 0xff) | ((b&1)<<8));
		break;
	}

	rtc.epoch = (un32)GW_GetUnixTime() - (rtc.s + (rtc.m * 60) + (rtc.h * 3600) + (rtc.d * 86400));
}
