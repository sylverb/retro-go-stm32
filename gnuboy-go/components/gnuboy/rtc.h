#ifndef __RTC_H__
#define __RTC_H__

#include <stdio.h>

#include "defs.h"

struct rtc
{
	un32 sel, latch;
	// Epoch time
	un32 epoch;
	// Current time
	un32 d, h, m, s, flags;
	// Latched time
	byte regs[8];
};

extern struct rtc rtc;

void rtc_latch(byte b);
void rtc_write(byte b);

#endif
