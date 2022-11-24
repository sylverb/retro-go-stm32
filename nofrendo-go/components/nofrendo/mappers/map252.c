#include "build/config.h"

#ifdef ENABLE_EMULATOR_NES
/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of version 2 of the GNU Library General
** Public License as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Library General Public License for more details.  To obtain a
** copy of the GNU Library General Public License, write to the Free
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** map021.c: VRC4 mapper interface
**
*/
#include <nofrendo.h>
#include <nes_mmc.h>
#include <nes.h>

static uint8 creg[8];

static void VRC_VBANK(uint32 address, uint8 value)
{
    uint8 bank = ((((address & 8) | (address >> 8)) >> 3) + 2) & 7;
    uint8 sar = address & 4;
    creg[bank] = (creg[bank] & (0xF0 >> sar)) | ((value & 0x0F) << sar);

    if (creg[bank] == 6 || creg[bank] == 7)
    {
        mmc_bankchr(1, (bank) << 10, creg[bank], CHR_RAM);
    }
    else
    {
        mmc_bankchr(1, (bank) << 10, creg[bank], CHR_ROM);
    }
};

static struct
{
    bool enabled, wait_state;
    int counter, latch;
} irq;




static void map_write(uint32 address, uint8 value)
{

	if( (address & 0xF000) == 0x8000 ) {
		 mmc_bankrom(8, 0x8000,value);
		return;
	};
	if( (address & 0xF000) == 0xA000 ) {
		mmc_bankrom(8, 0xA000,value); 
		return;
	};
    if ((address >= 0xB000) && (address <= 0xE00C))
    {
        VRC_VBANK(address,value); 
        return;
    }
    switch (address)
    {
    case 0x9000:
        switch (value & 3)
        {
            case 0: ppu_setmirroring(PPU_MIRROR_VERT); break;
            case 1: ppu_setmirroring(PPU_MIRROR_HORI); break;
            case 2: ppu_setmirroring(PPU_MIRROR_SCR0); break;
            case 3: ppu_setmirroring(PPU_MIRROR_SCR1); break;
        }
        break;        
    case 0xF000:
        irq.latch &= 0xF0;
        irq.latch |= (value & 0x0F);
        break;
    case 0xF004:
        irq.latch &= 0x0F;
        irq.latch |= ((value & 0x0F) << 4);
        break;
    case 0xF008:
        irq.enabled = (value >> 1) & 0x01;
        irq.wait_state = value & 0x01;
        irq.counter = irq.latch;
        break;
    case 0xF00C:
        irq.enabled = irq.wait_state;
        break;

    default:
        MESSAGE_DEBUG("wrote $%02X to $%04X", value, address);
        break;
    }
}

static void map_hblank(int scanline)
{
    if (irq.enabled)
    {
        if (256 == ++irq.counter)
        {
            irq.counter = irq.latch;
            nes6502_irq();
            //irq.enabled = false;
            irq.enabled = irq.wait_state;
        }
    }
}
static void map_getstate(uint8 *state)
{
    state[0] = irq.counter;
    state[1] = irq.enabled;
}

static void map_setstate(uint8 *state)
{
    irq.counter = state[0];
    irq.enabled = state[1];
}

static void map_init(void)
{
    irq.enabled = irq.wait_state = 0;
    irq.counter = irq.latch = 0;
}

static mem_write_handler_t map_memwrite[] =
{
   { 0x8000, 0xFFFF, map_write },
   LAST_MEMORY_HANDLER
};


mapintf_t map252_intf =
{
    .number     = 252,
    .name       = "Konami VRC4 A",
    .init       = map_init,
    .vblank     = NULL,
    .hblank     = map_hblank,
    .get_state  = map_getstate,
    .set_state  = map_setstate,
    .mem_read   = NULL,
    .mem_write  = map_memwrite,
    NULL,
};

#endif