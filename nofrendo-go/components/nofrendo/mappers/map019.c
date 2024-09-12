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
** map19.c
**
** mapper 19 interface
** $Id: map019.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include <nofrendo.h>
#include <nes_mmc.h>
#include <nes_ppu.h>

static struct
{
    uint16 counter;
    uint16 enabled;
} irq;

static rominfo_t *cart;
static int counter_inc;

static void map_write(uint32 address, uint8 value)
{
    int reg = address >> 11;
    uint8 *page;

    switch (reg)
    {
    case 0xA:
        irq.counter = (irq.counter & 0x7F00) | value;
        nes6502_irq_clear();
        break;

    case 0xB:
        irq.counter = (irq.counter & 0x00FF) | ((value & 0x7F) << 8);
        irq.enabled = (value & 0x80);
        nes6502_irq_clear();
        break;

    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
        mmc_bankvrom(1, (reg & 7) << 10, value);
        break;

    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
        if (value < 0xE0)
         page = &cart->vrom[(value % (cart->vrom_banks * 8)) << 10] - (0x2000 + ((reg & 3) << 10));
        else
            page = ppu_getnametable(value & 1) - (0x2000 + ((reg & 3) << 10));
        ppu_setpage(1, (reg & 3) + 8, page);
        ppu_setpage(1, (reg & 3) + 12, page);
        break;

    case 0x1C:
        mmc_bankrom(8, 0x8000, value);
        break;

    case 0x1D:
        mmc_bankrom(8, 0xA000, value);
        break;

    case 0x1E:
        mmc_bankrom(8, 0xC000, value);
        break;

    default:
        break;
    }
}

static uint8 map_read(uint32 address)
{
    int reg = address >> 11;

    switch (reg)
    {
    case 0xA:
        return irq.counter & 0xFF;

    case 0xB:
        return irq.counter >> 8;

    default:
        return 0xFF;
    }
}

static void map_hblank(int scanline)
{
    if (irq.enabled)
    {
      irq.counter += counter_inc;

        if (irq.counter >= 0x7FFF)
        {
            nes6502_irq();
            irq.counter = 0x7FFF;
            irq.enabled = 0;
        }
    }
}

static void map_getstate(uint8 *state)
{
    state[0] = irq.counter & 0xFF;
    state[1] = irq.counter >> 8;
    state[2] = irq.enabled;
}

static void map_setstate(uint8 *state)
{
    irq.counter = (state[1] << 8) | state[0];
    irq.enabled = state[2];
}

static void map_init(void)
{
   counter_inc = nes_getptr()->cycles_per_line;
   cart = mmc_getinfo();

    irq.counter = 0;
    irq.enabled = 0;
}

static mem_write_handler_t map_memwrite[] =
{
   { 0x5000, 0x5FFF, map_write },
   { 0x8000, 0xFFFF, map_write },
   LAST_MEMORY_HANDLER
};

static mem_read_handler_t map_memread[] =
{
   { 0x5000, 0x5FFF, map_read },
   LAST_MEMORY_HANDLER
};

mapintf_t map19_intf =
{
   19, /* mapper number */
   "Namco 129/163", /* mapper name */
   map_init, /* init routine */
   NULL, /* vblank callback */
   map_hblank, /* hblank callback */
   map_getstate, /* get state (snss) */
   map_setstate, /* set state (snss) */
   map_memread, /* memory read structure */
   map_memwrite, /* memory write structure */
   NULL /* external sound device */
};

#endif