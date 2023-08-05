#ifndef __NOISE_H__
#define __NOISE_H__


#include "defs.h"

/*DRAM_ATTR*/ static const byte noise7[] =
{
    0xfb,0xe7,0xae,0x1b,0xa6,0x2b,0x05,0xe3,
    0xb6,0x4a,0x42,0x72,0xd1,0x19,0xaa,0x03,
};

// nosie15 lookup table is populated in sound_init();
// This saves roughly 4056 bytes of extflash.
extern byte noise15[4096];


#endif
