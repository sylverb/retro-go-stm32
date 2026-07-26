// gfx.c - VDC/VCE Emulation
//
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "pce.h"
#include "gfx.h"

typedef struct
{
	uint16_t y; 	/* Vertical position */
	uint16_t x; 	/* Horizontal position */
	uint16_t no;   	/* Offset in VRAM */
	uint16_t attr; 	/* Attributes */
	/*
		* bit 0-4 : number of the palette to be used
		* bit 7 : background sprite
		*          0 -> must be drawn behind tiles
		*          1 -> must be drawn in front of tiles
		* bit 8 : width
		*          0 -> 16 pixels
		*          1 -> 32 pixels
		* bit 11 : horizontal flip
		*          0 -> normal shape
		*          1 -> must be draw horizontally flipped
		* bit 13-12 : height
		*          00 -> 16 pixels
		*          01 -> 32 pixels
		*          10 -> 48 pixels
		*          11 -> 64 pixels
		* bit 15 : vertical flip
		*          0 -> normal shape
		*          1 -> must be drawn vertically flipped
	*/
} sprite_t;

#define PAL(nibble) (PAL[(L >> ((nibble) * 4)) & 15])

#define V_FLIP  0x8000
#define H_FLIP  0x0800

static int last_line_counter = 0;
static int line_counter = 0;

static struct {
	int scroll_x;
	int scroll_y;
	int control;
	int latched;
} gfx_context;


/* Width of the active display area, clamped to XBUF_WIDTH.
 * Used in render loops so they never process pixels outside the framebuffer
 * (important when XBUF_WIDTH > IO_VDC_SCREEN_WIDTH, e.g. 512+32 buffer with
 * a 256-px game).
 */
static inline uint32_t
gfx_screen_width(void)
{
	uint32_t w = IO_VDC_SCREEN_WIDTH;
	return (w > XBUF_WIDTH) ? XBUF_WIDTH : w;
}

/*
	Draw background tiles between two lines
*/
static void // Do not inline
draw_tiles(uint8_t *screen_buffer, int Y1, int Y2, int scroll_x, int scroll_y)
{
	const uint8_t _bg_w[] = { 32, 64, 128, 128 };
	const uint8_t _bg_h[] = { 32, 64 };
	
	uint32_t bg_w = _bg_w[(IO_VDC_REG[MWR].W >> 4) & 3]; // Bits 5-4 select the width
    uint32_t bg_h = _bg_h[(IO_VDC_REG[MWR].W >> 6) & 1]; // Bit 6 selects the height

	int XW, no, x, y, h, offset;
	uint8_t *PP, *PAL, *P, *C;

	if (Y1 == 0) {
		TRACE_GFX("\n=================================================\n");
	}

	TRACE_GFX("Rendering lines %3d - %3d\tScroll: (%3d,%3d)\n", Y1, Y2, scroll_x, scroll_y);

	y = Y1 + scroll_y;
	offset = y & 7;
	h = 8 - offset;
	if (h > Y2 - Y1)
		h = Y2 - Y1;
	y >>= 3;

	PP = (screen_buffer + XBUF_WIDTH * Y1) - (scroll_x & 7);
	XW = gfx_screen_width() / 8 + 1;

	for (int Line = Y1; Line < Y2; y++) {
		x = scroll_x / 8;
		y &= bg_h - 1;
		for (int X1 = 0; X1 < XW; X1++, x++, PP += 8) {
			x &= bg_w - 1;

			no = PCE.VRAM[x + y * bg_w];

			PAL = &PCE.Palette[(no >> 8) & 0x1F0];

			// PCE has max of 2048 tiles
			no &= 0x7FF;

			C = (uint8_t*)(PCE.VRAM + no * 16 + offset);
			P = PP;
			for (int i = 0; i < h; i++, P += XBUF_WIDTH, C += 2) {
				uint32_t J, L, M;

				J = C[0] | C[1] | C[16] | C[17];

				if (!J)
					continue;

				M = C[0];
				L = ((M & 0x88) >> 3) | ((M & 0x44) << 6) | ((M & 0x22) << 15) | ((M & 0x11) << 24);
				M = C[1];
				L |= ((M & 0x88) >> 2) | ((M & 0x44) << 7) | ((M & 0x22) << 16) | ((M & 0x11) << 25);
				M = C[16];
				L |= ((M & 0x88) >> 1) | ((M & 0x44) << 8) | ((M & 0x22) << 17) | ((M & 0x11) << 26);
				M = C[17];
				L |= ((M & 0x88) >> 0) | ((M & 0x44) << 9) | ((M & 0x22) << 18) | ((M & 0x11) << 27);

				if (J & 0x80) P[0] = PAL(1);
				if (J & 0x40) P[1] = PAL(3);
				if (J & 0x20) P[2] = PAL(5);
				if (J & 0x10) P[3] = PAL(7);
				if (J & 0x08) P[4] = PAL(0);
				if (J & 0x04) P[5] = PAL(2);
				if (J & 0x02) P[6] = PAL(4);
				if (J & 0x01) P[7] = PAL(6);
			}
		}
		Line += h;
		PP += XBUF_WIDTH * h - XW * 8;
		offset = 0;
		h = Y2 - Line;
		if (h > 8)
			h = 8;
	}
}


/*
	Sprite rendering: PCE sprite-to-sprite priority is by SATB index (lowest
	index wins the pixel) ACROSS both priority classes, and the winning
	pixel's priority bit then decides sprite-vs-BG. Games rely on this to
	mask sprites behind BG windows (Ys I&II dialog box: low-index priority-0
	"mask" sprites hide the higher-index priority-1 heroes behind the BG).

	Like mednafen's spr_linebuf, sprites are first resolved into a buffer
	(entry = 0x100 | color byte, bit15 = priority) by drawing them from
	index 63 down to 0 so lower indexes overwrite. The buffer is then
	applied in two passes around the tiles: priority-0 winners before,
	priority-1 winners after. To keep memory bounded we process the render
	region in slices of SPR_SLICE_H lines.
*/
#define SPR_SLICE_H  16
#define SPR_BUF_W    (XBUF_WIDTH + 64)	/* 32px slack each side for offscreen sprites */
#define SPR_XOFS     32

static uint16_t spr_buf[SPR_SLICE_H][SPR_BUF_W];
static uint16_t spr_rmin[SPR_SLICE_H];	/* dirty x range per row (buffer coords) */
static uint16_t spr_rmax[SPR_SLICE_H];

/*
	Decode one 16px-wide sprite pattern row into the winner buffer B.
	C points at the row inside the pattern cell (planes at +0/+16/+32/+48).
	Writes overwrite unconditionally: the caller iterates sprites from 63
	down to 0, so the lowest index ends up winning each pixel.
*/
static void // Do not inline
draw_sprite_row(uint16_t *B, const uint16_t *C, uint16_t flags, uint16_t attr)
{
	uint8_t *PAL = &PCE.Palette[256 + ((attr & 0xF) << 4)];

	uint16_t J = C[0] | C[16] | C[32] | C[48];
	uint32_t L1, L2, L, M;

	if (!J)
		return;

	M = C[0];
	L1 = ((M & 0x88) >> 3) | ((M & 0x44) << 6) | ((M & 0x22) << 15) | ((M & 0x11) << 24);
	L2 = ((M & 0x8800) >> 11) | ((M & 0x4400) >> 2) | ((M & 0x2200) << 7) | ((M & 0x1100) << 16);
	M = C[16];
	L1 |= ((M & 0x88) >> 2) | ((M & 0x44) << 7) | ((M & 0x22) << 16) | ((M & 0x11) << 25);
	L2 |= ((M & 0x8800) >> 10) | ((M & 0x4400) >> 1) | ((M & 0x2200) << 8) | ((M & 0x1100) << 17);
	M = C[32];
	L1 |= ((M & 0x88) >> 1) | ((M & 0x44) << 8) | ((M & 0x22) << 17) | ((M & 0x11) << 26);
	L2 |= ((M & 0x8800) >> 9) | ((M & 0x4400) >> 0) | ((M & 0x2200) << 9) | ((M & 0x1100) << 18);
	M = C[48];
	L1 |= ((M & 0x88) >> 0) | ((M & 0x44) << 9) | ((M & 0x22) << 18) | ((M & 0x11) << 27);
	L2 |= ((M & 0x8800) >> 8) | ((M & 0x4400) << 1) | ((M & 0x2200) << 10) | ((M & 0x1100) << 19);

	if (attr & H_FLIP) {
		L = L2;
		if ((J & 0x8000)) B[15] = flags | PAL(1);
		if ((J & 0x4000)) B[14] = flags | PAL(3);
		if ((J & 0x2000)) B[13] = flags | PAL(5);
		if ((J & 0x1000)) B[12] = flags | PAL(7);
		if ((J & 0x0800)) B[11] = flags | PAL(0);
		if ((J & 0x0400)) B[10] = flags | PAL(2);
		if ((J & 0x0200)) B[9]  = flags | PAL(4);
		if ((J & 0x0100)) B[8]  = flags | PAL(6);

		L = L1;
		if ((J & 0x80)) B[7] = flags | PAL(1);
		if ((J & 0x40)) B[6] = flags | PAL(3);
		if ((J & 0x20)) B[5] = flags | PAL(5);
		if ((J & 0x10)) B[4] = flags | PAL(7);
		if ((J & 0x08)) B[3] = flags | PAL(0);
		if ((J & 0x04)) B[2] = flags | PAL(2);
		if ((J & 0x02)) B[1] = flags | PAL(4);
		if ((J & 0x01)) B[0] = flags | PAL(6);
	}
	else {
		L = L2;
		if ((J & 0x8000)) B[0] = flags | PAL(1);
		if ((J & 0x4000)) B[1] = flags | PAL(3);
		if ((J & 0x2000)) B[2] = flags | PAL(5);
		if ((J & 0x1000)) B[3] = flags | PAL(7);
		if ((J & 0x0800)) B[4] = flags | PAL(0);
		if ((J & 0x0400)) B[5] = flags | PAL(2);
		if ((J & 0x0200)) B[6] = flags | PAL(4);
		if ((J & 0x0100)) B[7] = flags | PAL(6);

		L = L1;
		if ((J & 0x80)) B[8]  = flags | PAL(1);
		if ((J & 0x40)) B[9]  = flags | PAL(3);
		if ((J & 0x20)) B[10] = flags | PAL(5);
		if ((J & 0x10)) B[11] = flags | PAL(7);
		if ((J & 0x08)) B[12] = flags | PAL(0);
		if ((J & 0x04)) B[13] = flags | PAL(2);
		if ((J & 0x02)) B[14] = flags | PAL(4);
		if ((J & 0x01)) B[15] = flags | PAL(6);
	}
}


/*
	Resolve all sprites of the slice [Y1,Y2) into spr_buf.
	Returns true if at least one priority-1 (foreground) pixel was written.
*/
static bool // Do not inline
sprites_decode_slice(int Y1, int Y2)
{
	bool has_prio1 = false;

	/* Clear only the ranges dirtied by the previous slice */
	for (int i = 0; i < SPR_SLICE_H; i++) {
		if (spr_rmax[i] > spr_rmin[i])
			memset(&spr_buf[i][spr_rmin[i]], 0, (spr_rmax[i] - spr_rmin[i]) * sizeof(uint16_t));
		spr_rmin[i] = SPR_BUF_W;
		spr_rmax[i] = 0;
	}

	for (int n = 63; n >= 0; n--) {
		sprite_t *spr = (sprite_t *)PCE.SPRAM + n;
		uint16_t attr = spr->attr;

		int y = (spr->y & 0x3FF) - 64;
		int x = (spr->x & 0x3FF) - 32;
		int cgx = (attr >> 8) & 1;
		int cgy = (attr >> 12) & 3;
		int no = (spr->no & 0x7FF);

		TRACE_SPR("Sprite 0x%02X : X = %d, Y = %d, attr = %d, no = %d\n", n, x, y, attr, no);

		cgy |= cgy >> 1;
		no = (no >> 1) & ~(cgy * 2 + cgx);

		// PCE has max of 512 sprites
		no &= 0x1FF;

		int height = (cgy + 1) * 16;

		if (y >= Y2 || y + height <= Y1 || x >= (int)gfx_screen_width() || x + (cgx + 1) * 16 < 0) {
			continue;
		}

		/* bit15 = in front of BG, bit8 = opaque marker */
		uint16_t flags = 0x100 | ((attr & 0x80) ? 0x8000 : 0);
		if (flags & 0x8000)
			has_prio1 = true;

		const uint16_t *C = PCE.VRAM + (no * 64);

		int r0 = (y < Y1) ? Y1 : y;
		int r1 = (y + height > Y2) ? Y2 : (y + height);

		for (int r = r0; r < r1; r++) {
			int yo = r - y;
			if (attr & V_FLIP)
				yo = height - 1 - yo;

			/* vertical cell stride is 128 words (mednafen: no |= (y_offset & 0x30) >> 3) */
			const uint16_t *Crow = C + ((yo >> 4) * 128) + (yo & 15);
			uint16_t *B = &spr_buf[r - Y1][SPR_XOFS + x];

			for (int j = 0; j <= cgx; j++) {
				const uint16_t *cell = Crow + (((attr & H_FLIP) ? (cgx - j) : j) * 64);
				draw_sprite_row(B + j * 16, cell, flags, attr);
			}

			int bx0 = SPR_XOFS + x;
			int bx1 = bx0 + (cgx + 1) * 16;
			if (bx0 < spr_rmin[r - Y1]) spr_rmin[r - Y1] = bx0;
			if (bx1 > spr_rmax[r - Y1]) spr_rmax[r - Y1] = bx1;
		}
	}

	return has_prio1;
}


/*
	Blit the sprite winners of one priority class to the framebuffer.
	prio=0 must be called before draw_tiles, prio=1 after.
*/
static void // Do not inline
sprites_apply(uint8_t *screen_buffer, int Y1, int Y2, int prio)
{
	int width = gfx_screen_width();

	for (int r = 0; r < Y2 - Y1; r++) {
		int bmin = spr_rmin[r], bmax = spr_rmax[r];
		if (bmin >= bmax)
			continue;

		int x0 = bmin - SPR_XOFS;
		int x1 = bmax - SPR_XOFS;
		if (x0 < 0) x0 = 0;
		if (x1 > width) x1 = width;

		uint8_t *fb = screen_buffer + (Y1 + r) * XBUF_WIDTH;
		const uint16_t *B = &spr_buf[r][SPR_XOFS];

		if (prio) {
			for (int x = x0; x < x1; x++) {
				uint16_t e = B[x];
				if (e & 0x8000)
					fb[x] = (uint8_t)e;
			}
		} else {
			for (int x = x0; x < x1; x++) {
				uint16_t e = B[x];
				if (e && !(e & 0x8000))
					fb[x] = (uint8_t)e;
			}
		}
	}
}


/*
	Hit Check Sprite#0 and others
*/
static inline bool
sprite_hit_check(void)
{
	sprite_t *spr = (sprite_t *)PCE.SPRAM;
	int x0 = spr->x;
	int y0 = spr->y;
	int w0 = (((spr->attr >> 8) & 1) + 1) * 16;
	int h0 = (((spr->attr >> 12) & 3) + 1) * 16;

	spr++;

	for (int i = 1; i < 64; i++, spr++) {
		int x = spr->x;
		int y = spr->y;
		int w = (((spr->attr >> 8) & 1) + 1) * 16;
		int h = (((spr->attr >> 12) & 3) + 1) * 16;
		if ((x < x0 + w0) && (x + w > x0) && (y < y0 + h0) && (y + h > y0))
			return 1;
	}
	return 0;
}


void gfx_latch_context(int force)
{
	if (!gfx_context.latched || force) { // Context is already saved + we haven't render the line using it
		gfx_context.scroll_x = IO_VDC_REG[BXR].W;
		/* Mid-frame BYR strips (Ys I&II village) need BYR-1 vs this core's
		 * line counter or the last line of each strip hits a transparent
		 * tile row. Skip when base scroll is 0: BYR=0 with -1 becomes -1 and
		 * draw_tiles wraps to the BAT bottom row (dotted first line on Ys III
		 * black screens after System Card). */
		int sy = (int)IO_VDC_REG[BYR].W - PCE.ScrollYDiff;
		if (sy > 0)
			sy -= 1;
		gfx_context.scroll_y = sy;
		gfx_context.control = IO_VDC_REG[CR].W;
		gfx_context.latched = 1;
	}
}


/*
	Render lines into the buffer for y in [min_line, max_line).
*/
static inline void
render_lines(int min_line, int max_line)
{
	gfx_context.latched = 0;

	uint8_t *screen_buffer = osd_gfx_framebuffer();
	if (!screen_buffer) {
		return;
	}

	/* Clear only the lines we draw. Inclusive clear of max_line used to wipe
	 * the first line of the next raster strip before it was redrawn. */
	size_t screen_width = gfx_screen_width();
	for (int y = min_line; y < max_line; y++) {
		memset(screen_buffer + (y * XBUF_WIDTH), PCE.Palette[0], screen_width);
	}

	/* Process the region in slices so the sprite winner buffer stays small.
	   For each slice: resolve sprite priorities, blit background sprites,
	   draw the tiles over them, then blit foreground sprites. */
	for (int Y1 = min_line; Y1 < max_line; Y1 += SPR_SLICE_H) {
		int Y2 = Y1 + SPR_SLICE_H;
		if (Y2 > max_line)
			Y2 = max_line;

		bool has_prio1 = false;

		if (gfx_context.control & 0x40) {
			has_prio1 = sprites_decode_slice(Y1, Y2);
			sprites_apply(screen_buffer, Y1, Y2, 0);
		}

		if (gfx_context.control & 0x80) {
			draw_tiles(screen_buffer, Y1, Y2, gfx_context.scroll_x, gfx_context.scroll_y);
		}

		if (has_prio1) {
			sprites_apply(screen_buffer, Y1, Y2, 1);
		}
	}
}


int
gfx_init(void)
{
	gfx_reset(true);
	return 0;
}


void
gfx_reset(bool hard)
{
	last_line_counter = 0;
	line_counter = 0;
	if (hard) {
		gfx_context.latched = 0;
		gfx_context.scroll_x = 0;
		gfx_context.scroll_y = 0;
		gfx_context.control = 0;
	}
}


void
gfx_term(void)
{
	//
}


/*
	Raises a VDC IRQ and/or process pending VDC IRQs.
	More than one interrupt can happen in a single line on real hardware and the cpu
	would usually receive them one by one. We use an uint32 as a 8 slot buffer.
*/
void
gfx_irq(int type)
{
	/* If IRQ, push it on the stack */
	if (type >= 0) {
		PCE.VDC.pending_irqs <<= 4;
		PCE.VDC.pending_irqs |= (1+type) & 0xF;
	}

	/* Pop the first pending vdc interrupt only if CPU_PCE.irq_lines is clear */
	int pos = 28;
	while (!(CPU_PCE.irq_lines & INT_IRQ1) && PCE.VDC.pending_irqs) {
		if (PCE.VDC.pending_irqs >> pos) {
			PCE.VDC.status |= 1 << ((PCE.VDC.pending_irqs >> pos)-1);
			PCE.VDC.pending_irqs &= ~(0xF << pos);
			CPU_PCE.irq_lines |= INT_IRQ1; // Notify the CPU_PCE
		}
		pos -= 4;
	}
}


/*
	VRAM to VRAM DMA: transfer ~one scanline worth of words.
	Mednafen (vdc.cpp DoDMA) runs 455 bus cycles per free scanline, one word
	needing a read cycle + a write cycle => ~227 words per scanline.
*/
static void
vram_dma_run_chunk(void)
{
	int src_inc = (IO_VDC_REG[DCR].W & 8) ? -1 : 1;
	int dst_inc = (IO_VDC_REG[DCR].W & 4) ? -1 : 1;

	for (int i = 0; i < 227; i++) {
		if (IO_VDC_REG[DISTR].W < 0x8000) {
			PCE.VRAM[IO_VDC_REG[DISTR].W] = PCE.VRAM[IO_VDC_REG[SOUR].W];
		}
		IO_VDC_REG[SOUR].W += src_inc;
		IO_VDC_REG[DISTR].W += dst_inc;
		IO_VDC_REG[LENR].W -= 1;
		if (IO_VDC_REG[LENR].W == 0xFFFF) {
			PCE.VDC.vram = 0;
			if (DMAIntON) //generate the interrupt when requested
				gfx_irq(VDC_STAT_DV);
			break;
		}
	}
}


/*
	Process one scanline
*/
void
gfx_run(void)
{
	int scanline = PCE.Scanline;
	bool need_vbi = false;

    if (scanline == 0)
	{
		PCE.VBlankFL = IO_VDC_MAXLINE + 1;
   		if(PCE.VBlankFL > 261)
    		PCE.VBlankFL = 261;

		/* Apply pending VDC timing before the first visible line is drawn. */
		if (PCE.VDC.mode_chg) {
			PCE.VDC.mode_chg = 0;
			osd_gfx_set_mode(IO_VDC_SCREEN_WIDTH, IO_VDC_SCREEN_HEIGHT);
		}
	}


	if( scanline == PCE.VBlankFL ){
		if (VBlankON) {
			need_vbi = true;
		}
		/* SAT DMA fires at VBlankFL (mednafen vdc.cpp ~L1321), not at the fixed
		 * scanline 256 below — that way sprites are always updated at the right
		 * moment regardless of the VDC vertical timing configuration. */
		if (PCE.VDC.satb == DMA_TRANSFER_PENDING || IO_VDC_REG[DCR].W & 0x0010) {
			memcpy(PCE.SPRAM, PCE.VRAM + IO_VDC_REG[SATB].W, 512);
			PCE.VDC.satb = DMA_TRANSFER_COUNTER + 4;
		}
	}



	/* Test raster hit */
	if (RasHitON) {
		if ( IO_VDC_REG[RCR].W >= 0x40 && (IO_VDC_REG[RCR].W <= 0x146))
		{
			uint16_t temp_rcr = (uint16_t)(IO_VDC_REG[RCR].W - 0x40);
			if (scanline == (temp_rcr + IO_VDC_MINLINE) % 263)
			{
				TRACE_GFX("\n-----------------RASTER HIT (%d)------------------\n", scanline);
				gfx_irq(VDC_STAT_RR);				
			}
		}

	}

	/* VRAM to VRAM DMA gets a slice of every scanline where the VDC is not
	 * fetching the display: all blanking lines, plus "burst mode" (both BG
	 * and sprites disabled). Mednafen runs DoDMA on every such line; doing
	 * it only once per frame (old code, scanline 256 only) made big
	 * transfers ~40x too slow and delayed the DV IRQ by dozens of frames. */
	if (PCE.VDC.vram == DMA_TRANSFER_PENDING) {
		bool display_active = (scanline >= 14 && scanline <= 255)
			&& (scanline >= IO_VDC_MINLINE && scanline <= IO_VDC_MAXLINE)
			&& (IO_VDC_REG[CR].W & 0xC0);
		if (!display_active) {
			vram_dma_run_chunk();
		}
	}

	int32_t line_leadin1 = 0;
	int32_t magical = M_vdc_HDS + (M_vdc_HDW + 1) + M_vdc_HDE;
	magical = (magical + 2) & ~1;
	magical -= M_vdc_HDW + 1;
	int32_t cyc_tot = magical * 8; 
	cyc_tot-=2;
	switch(PCE.VCE.dot_clock)
	{
		case 0: cyc_tot = 4 * cyc_tot / 3; break;
		case 1: break;
		case 2: cyc_tot = 2 * cyc_tot / 3; break;
	}

	if(cyc_tot < 0) cyc_tot = 0;
	line_leadin1 = cyc_tot;


	h6280_run(line_leadin1);

	/* Visible area */
	if (scanline >= 14 && scanline <= 255) {
		if (scanline == IO_VDC_MINLINE) {
			gfx_latch_context(1);
		}

		if (scanline >= IO_VDC_MINLINE && scanline <= IO_VDC_MAXLINE) {
			if (gfx_context.latched) {
				render_lines(last_line_counter, line_counter);
				last_line_counter = line_counter;
			}
			line_counter++;
		}
	}
	/* V Blank trigger line (scanline 256 = first line after the visible area
	 * range 14-255). SAT DMA is handled above at VBlankFL; VRAM DMA and other
	 * end-of-frame work stay here. */
	else if (scanline == 256) {

		/* Flush pending mode before the last render batch so stale pixels from
		 * the previous FB offset cannot flash when the VDC viewport changes. */
		if (PCE.VDC.mode_chg) {
			PCE.VDC.mode_chg = 0;
			osd_gfx_set_mode(IO_VDC_SCREEN_WIDTH, IO_VDC_SCREEN_HEIGHT);
		}

		// Draw any lines left in the context
		gfx_latch_context(0);
		render_lines(last_line_counter, line_counter);

		// Trigger interrupts
		if (SpHitON && sprite_hit_check()) {
			gfx_irq(VDC_STAT_CR);
		}

		/* VRAM DMA is handled above, once per blanking scanline. */
	}
	/* V Blank area */
	else {
		gfx_context.latched = 0;
		last_line_counter = 0;
		line_counter = 0;
		PCE.ScrollYDiff = 0;		
	}

	if ( need_vbi ){
		PCE.VDC.status |= 1 << VDC_STAT_VD;
	}

	h6280_run(2);

	if ( IO_VDC_STATUS(VDC_STAT_VD) ){
		CPU_PCE.irq_lines |= INT_IRQ1;
	}
	
	h6280_run(PCE.Timer.cycles_per_line - 82 - 2);

	/* DMA Transfer in "progress" */
	if (PCE.VDC.satb > DMA_TRANSFER_COUNTER) {
		if (--PCE.VDC.satb == DMA_TRANSFER_COUNTER) {
			PCE.VDC.satb = 0;
			if (SATBIntON) {
				gfx_irq(VDC_STAT_DS);
			}
		}
	}


	/* Always call at least once (to handle pending IRQs) */
	//gfx_irq(-1);
}
