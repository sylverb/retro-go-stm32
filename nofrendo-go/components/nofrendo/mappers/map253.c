#if FORCE_NOFRENDO == 1
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

static uint8 chr[8];
//static uint8 chrhi[8];
//static uint8 vlock;

// static void VRC_VBANK(uint32 address, uint8 value)
// {

//     uint8 bank = ((((address & 8) | (address >> 8)) >> 3) + 2) & 7;
//     uint8 sar = address & 4;
//     uint8 clo = (chrlo[bank] & (0xF0 >> sar)) | ((value & 0x0F) << sar);
//     chrlo[bank] = clo;
//     if (bank == 0)
//     {
//         if (clo == 0xc8)
//             vlock = 0;
//         else if (clo == 0x88)
//             vlock = 1;
//     }
//     if (sar)
//         chrhi[bank] = value >> 4;

//     uint32 chr = chrlo[bank] | (chrhi[bank] << 8);

//     if (chrlo[bank] == 4 || chrlo[bank] == 5)
//     {
//         mmc_bankchr(1, (bank) << 10, chr, CHR_RAM);
//     }
//     else
//     {
//         mmc_bankchr(1, (bank) << 10, chr, CHR_ROM);
//     }
// };

static struct
{
    bool enabled, wait_state;
    int counter, latch;
} irq;

void mapper253_SetBank(int bank, int page)
{
    if ((page == 4) || (page == 5))
    {
        mmc_bankchr(1, bank << 10, page, CHR_RAM);
    }
    else
    {
        mmc_bankchr(1, bank << 10, page, CHR_ROM);
    }
}

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
    switch (address & 0xF00C)
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
    case 0xB000:
        chr[0] = (chr[0] & 0xF0) | (value & 0x0F);
        mapper253_SetBank(0, chr[0]);
        break;
    case 0xB004:
        chr[0] = (chr[0] & 0x0F) | ((value & 0x0F) << 4);
        mapper253_SetBank(0, chr[0] + ((value >> 4) * 0x100));
        break;
    case 0xB008:
        chr[1] = (chr[1] & 0xF0) | (value & 0x0F);
        mapper253_SetBank(1, chr[1]);
        break;
    case 0xB00C:
        chr[1] = (chr[1] & 0x0F) | ((value & 0x0F) << 4);
        mapper253_SetBank(1, chr[1] + ((value >> 4) * 0x100));
        break;
    case 0xC000:
        chr[2] = (chr[2] & 0xF0) | (value & 0x0F);
        mapper253_SetBank(2, chr[2]);
        break;
    case 0xC004:
        chr[2] = (chr[2] & 0x0F) | ((value & 0x0F) << 4);
        mapper253_SetBank(2, chr[2] + ((value >> 4) * 0x100));
        break;
    case 0xC008:
        chr[3] = (chr[3] & 0xF0) | (value & 0x0F);
        mapper253_SetBank(3, chr[3]);
        break;
    case 0xC00C:
        chr[3] = (chr[3] & 0x0F) | ((value & 0x0F) << 4);
        mapper253_SetBank(3, chr[3] + ((value >> 4) * 0x100));
        break;
    case 0xD000:
        chr[4] = (chr[4] & 0xF0) | (value & 0x0F);
        mapper253_SetBank(4, chr[4]);
        break;
    case 0xD004:
        chr[4] = (chr[4] & 0x0F) | ((value & 0x0F) << 4);
        mapper253_SetBank(4, chr[4] + ((value >> 4) * 0x100));
        break;
    case 0xD008:
        chr[5] = (chr[5] & 0xF0) | (value & 0x0F);
        mapper253_SetBank(5, chr[5]);
        break;
    case 0xD00C:
        chr[5] = (chr[5] & 0x0F) | ((value & 0x0F) << 4);
        mapper253_SetBank(5, chr[5] + ((value >> 4) * 0x100));
        break;
    case 0xE000:
        chr[6] = (chr[6] & 0xF0) | (value & 0x0F);
        mapper253_SetBank(6, chr[6]);
        break;
    case 0xE004:
        chr[6] = (chr[6] & 0x0F) | ((value & 0x0F) << 4);
        mapper253_SetBank(6, chr[6] + ((value >> 4) * 0x100));
        break;
    case 0xE008:
        chr[7] = (chr[7] & 0xF0) | (value & 0x0F);
        mapper253_SetBank(7, chr[7]);
        break;
    case 0xE00C:
        chr[7] = (chr[7] & 0x0F) | ((value & 0x0F) << 4);
        mapper253_SetBank(7, chr[7] + ((value >> 4) * 0x100));
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

mapintf_t map253_intf =
{
    .number     = 253,
    .name       = "Konami VRC4 B",
    .init       = NULL,
    .vblank     = NULL,
    .hblank     = map_hblank,
    .get_state  = map_getstate,
    .set_state  = map_setstate,
    .mem_read   = NULL,
    .mem_write  = map_memwrite,
	NULL,
};

#endif