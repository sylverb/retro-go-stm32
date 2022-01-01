#ifndef _GAME_GENIE_H_
#define _GAME_GENIE_H_

#include <stddef.h>

void gameGenieInitialize(const char **codes, int numCodes);
void gameGenieShutdown();
void gameGeniePatchRom(unsigned char (*getbyte)(unsigned int addr), void (*putbyte)(unsigned int addr, unsigned char val));

#endif
