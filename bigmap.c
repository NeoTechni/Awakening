//
//  Big map scrolling and sprite animation
//	documentation https://pastebin.com/rXWxjYiF
//
//! \file bigmap.c
//! \author J Vijn
//! \date 20060508 - 20070216
//

#include <stdio.h>
#include <string.h>
#include <tonc.h>

#include "api.h"
#include "link.h"

#include "kakariko.h"
#include "link_gfx.h"


//#define kakarikoMapPitch 128

// === CONSTANTS ======================================================
// === CLASSES ========================================================

typedef struct VIEWPORT{
	int x, xmin, xmax, xpage;
	int y, ymin, ymax, ypage;
} VIEWPORT;

typedef struct TMapInfo{
	union {
		u32 state;			//!< Background state
		struct {
			u16 flags;
			u16 cnt;
		};
	};
	// Destination data
	SCR_ENTRY *dstMap;		//!< Screenblock pointer	
	// Source data
	SCR_ENTRY *srcMap;		//!< Source map address
	u32 srcMapWidth;		//!< Source map width
	u32 srcMapHeight;		//!< Source map height
	FIXED mapX;				//!< X-coord on map (.8f)
	FIXED mapY;				//!< Y-coord on map (.8f)
} TMapInfo;


// === GLOBALS ========================================================

int map_main_x, map_main_y;

VIEWPORT g_vp = {//1024 = 32*32 (tiles in the viewport)
	0, 0, 1024, SCREEN_WIDTH, 	//x, xmin, xmax, xpage;
	0, 0, 1024, SCREEN_HEIGHT, 	//y, ymin, ymax, ypage;
};

TMapInfo g_bg;

OBJ_ATTR obj_buffer[128];

TSprite g_link;
bool		did_init = false;


// === PROTOTYPES =====================================================

INLINE void vp_center(VIEWPORT *vp, FIXED x, FIXED y);
void vp_set_pos(VIEWPORT *vp, FIXED x, FIXED y);

// === MACROS =========================================================
// === INLINES=========================================================
// === FUNCTIONS ======================================================


// --- VIEWPORT ---

INLINE void vp_center(VIEWPORT *vp, int x, int y){	
	vp_set_pos(vp, x - vp->xpage / 2, y - vp->ypage / 2);	
	//vp_set_pos(vp, x - vp->xpage / 2, y - 80);
}

void vp_set_pos(VIEWPORT *vp, int x, int y){
	vp->x				= clamp(x, vp->xmin, vp->xmax - vp->xpage);
	vp->y				= clamp(y, vp->ymin, vp->ymax - vp->ypage);
}


// --- TMapInfo ---

void bgt_init(TMapInfo *bgt, int bgnr, u32 ctrl, const void *map, u32 map_width, u32 map_height, u32 link_x, u32 link_y) {
	memset(bgt, 0, sizeof(TMapInfo));

	bgt->flags			= bgnr;
	bgt->cnt			= ctrl;
	bgt->dstMap			= se_mem[BFN_GET(ctrl, BG_SBB)];			// One 256x256p map takes up a single screenblock

	REG_BGCNT[bgnr]		= ctrl;
	REG_BG_OFS[bgnr].x	= 0;
	REG_BG_OFS[bgnr].y	= 0;

	bgt->srcMap			= (SCR_ENTRY*)map;
	bgt->srcMapWidth	= map_width;
	bgt->srcMapHeight	= map_height;
	
	bgt->mapX			= link_x * 8 + 1;//was << 8
	bgt->mapY			= link_y * 8 + 1;
	
	link_x				= MAX(grid(link_x) - 16, 0);
	link_y				= MAX(grid(link_y) - 16, 0);
	
	int ix, iy;
	SCR_ENTRY *dst= bgt->dstMap, *src= bgt->srcMap;
	for(iy = link_y; iy < link_y + 32; iy++){
		for(ix = link_x; ix < link_x + 32; ix++){
			dst[iy *32 + ix] = src[iy * bgt->srcMapWidth + ix];
		}
	}
}

/*
    Write one 32t column of SCR_ENTRIES from srcMap to dstMap.
    Called when the viewport has moved to a new tile coordinate towards the left or right this frame.
 
    Parameters: The tile coordinates (in range [srcWidth, srcHeight]) of either
        a) the top left corner, or
        b) the top right corner
    of the viewport window.
 
    The column is retrieved from srcMap tile coords:
		(x = src_tile_x, y in [src_tile_y, src_tile_y + 31]).
 
    It's copied over to:
		(x = dst_tile_x, y in [0, 31]).
*/
void bgt_colcpy(TMapInfo *bgt, int tx, int ty){
	int iy, y0= ty&31;

	int srcP			= bgt->srcMapWidth;							//pitch
	SCR_ENTRY *srcL		= &bgt->srcMap[ty*srcP + tx];				// SCR_ENTRY of the newly exposed tile (top left/right of viewport) in srcMap
	SCR_ENTRY *dstL		= &bgt->dstMap[y0*32 + (tx&31)];			// SCR_ENTRY in dstMap (left/rightmost edge of the viewport offset) to write the new tile to

	for(iy=y0; iy<32; iy++){	
		*dstL			= *srcL;									// Write a single tile
		dstL 			+= 32;										// Move dst_entry pointer down one tile (dst_tile_y++)
		srcL 			+= srcP;									// Move src_entry pointer down one tile (src_tile_y++)
	}

	dstL 				-= 1024;									// Move dst_entry pointer up to y = 0

	for(iy=0; iy<y0; iy++){											// Copy over every tile to dstMap for y range [0, dst_tile_y - 1]
		*dstL			= *srcL;
		dstL 			+= 32;
		srcL 			+= srcP;	
	}
}

/*
    Write one 32t row of SCR_ENTRIES from srcMap to dstMap.
    Called when the viewport has moved up or down one tile coordinate this frame.
 
    Parameters: The tile coordinates (in range [srcWidth, srcHeight]) of either
        a) the top left corner, or
        b) the bottom left corner
    of the viewport window.
 
    The row is retrieved from srcMap tile coords:
		(x in [src_tile_x, src_tile_x + 31], y = src_tile_y).
 
    It's copied over to:
		(x in [0, 31], y = dst_tile_y).
*/
void bgt_rowcpy(TMapInfo *bgt, int tile_x, int tile_y){
	// Find a corresponding row in the 32x32t space of dstMap (same as % 32)
	int ix				= tile_y & 31;
	int x0				= tile_x & 31;

	int width			= bgt->srcMapWidth;							//pitch
	SCR_ENTRY *srcL		= &bgt->srcMap[tile_y * width + tile_x];	// SCR_ENTRY of the newly exposed tile (top/bottom left of viewport) in srcMap
	SCR_ENTRY *dstL		= &bgt->dstMap[ix * 32 + x0];				// SCR_ENTRY in dstMap (top/bottom edge of the viewport offset) to write the new tile to

	for(ix = x0; ix < 32; ix++){									// Write every tile to dstMap from x in [dst_tile_x, 31]
		*dstL			= *srcL;
		dstL			++;		
		srcL			++;
	}
	
	dstL 				-= 32;										// Move dst_entry back to x = 0

	for(ix = 0; ix < x0; ix++){										// Write every tile to dstMap from x in [0, dst_tile_x - 1]
		*dstL			= *srcL;
		dstL			++;
		srcL			++;
	}
}

/* 
    Check whether the viewport has shifted to another tile (up/down/left/right) this frame.
    If it has, copy one 32t row/column of srcMap over to dstMap to give the appearance of a larger continuously scrolling map.
 
    Since regular backgrounds wrap around by default, REG_BG_OFS (the viewport offset) is modulo
    256x256p (dimensions of dstMap). We can thus write the newly exposed row/column from srcMap
    to the row/column in 32x32t dstMap space that is just outside the dimensions of the
    viewport (in the direction of the shift).
 
    The result is that the viewport will always show a 240x160p slice of dstMap that corresponds to the "camera's position" in srcMap (centred around the player's position).
*/
void bgt_update(TMapInfo *bgt, VIEWPORT *vp){
	// Pixel coords
	int vx= vp->x, vy= vp->y;
	int bx= bgt->mapX, by= bgt->mapY;

	// Tile coordinates of the viewport in srcMap (these should all divide by 8)
	int tvx= vx>>3, tvy= vy>>3;
	int tbx= bx>>3, tby= by>>3;

	if (tvx < tbx){
		bgt_colcpy(bgt, tvx, tvy);									// Append from left side of srcMap
	} else if (tvx > tbx){
		bgt_colcpy(bgt, tvx + 31, tvy);								// Append from right side of srcMap
	}

	if(tvy < tby){
		bgt_rowcpy(bgt, tvx, tvy);									// Append from top row of srcMap
	} else if (tvy > tby){
		bgt_rowcpy(bgt, tvx, tvy + 27);								// Append from bottom row of srcMap
	}
	
	// Update TMapInfo and reg-offsets
	int bgnr 			= bgt->flags;
	REG_BG_OFS[bgnr].x 	= bgt->mapX= vx;
	REG_BG_OFS[bgnr].y 	= bgt->mapY= vy;
}

void init_textbox(int bgnr, int left, int top, int right, int bottom) {	
	tte_set_margins(left, top, right, bottom);

	REG_DISPCNT 		|= DCNT_WIN0;

	REG_WIN0H			= left<<8 | right;
	REG_WIN0V			= top<<8 | bottom;
	REG_WIN0CNT			= WIN_ALL | WIN_BLD;
	REG_WINOUTCNT		= WIN_ALL;

	REG_BLDCNT			= (BLD_ALL&~BIT(bgnr)) | BLD_BLACK;
	REG_BLDY			= 5;
}

bool init_map_main(const void *colorpallet, u32 colorpalletLen, const void *tileset, u32 tilesetLen, const void *map, u32 map_width, u32 map_height, u32 link_x, u32 link_y) {
	// Basic setups
	irq_init(NULL);
	irq_add(II_VBLANK, NULL);
	oam_init(obj_buffer, 128);
	clearoccupied(map_width, map_height);

	// Bigmap setup
	if (tilesetLen == 0){
		LZ77UnCompVram(tileset, tile_mem[0]);						//LZW compressed tiles
	} else {
		GRIT_CPY(tile_mem[0], tileset);								//uncompressed tiles
	}
	GRIT_CPY(pal_bg_mem, colorpallet);								//color pallet
	
	bgt_init(&g_bg, 1, BG_CBB(0)|BG_SBB(29), map, map_width, map_height, link_x, link_y);	//map

	mapdata 			= (short unsigned *) map;
	tiledata			= (short unsigned *) tileset;
	map_main_width 		= map_width;
	map_main_height 	= map_height;
	g_vp.xmax			= map_width * 8;
	g_vp.ymax 			= map_height * 8;

	// Object setup
	GRIT_CPY(pal_obj_mem, link_gfxPal);
	GRIT_CPY(tile_mem[4], link_gfxTiles);

	//					X         Y          OBJECT ID
	link_init(&g_link, link_x<<8, link_y<<8, 0);					//link

	//# NOTE: erasing and rendering text flows over into the VDRAW period.
	//# Using the ASM renderer and placing the text at the bottom limits its effects.
	tte_init_chr4c_b4_default(0, BG_CBB(2)|BG_SBB(28));
	tte_set_drawg(chr4c_drawg_b4cts_fast);
	tte_init_con();

	init_textbox(0, 8, SCR_H-(8+2*12), SCR_W-8, SCR_H-8);
	REG_DISPCNT 		= DCNT_MODE0 | DCNT_BG0 | DCNT_BG1 | DCNT_OBJ | DCNT_OBJ_1D | DCNT_WIN0;
	
	REG_BG1CNT 			|= 128;
	did_init			= true;
	return 				true;
}

int map_main() {
	did_init			= false;
	link_input(&g_link);
	link_animate(&g_link);
	link_move(&g_link);

	map_main_x 			= g_link.x >> 8;
	map_main_y			= g_link.y >> 8;
	vp_center(&g_vp, map_main_x, map_main_y);
	oam_copy(oam_mem, obj_buffer, 128);
	
	bgt_update(&g_bg, &g_vp);
	tte_printf("#{es;P}(x, y) = (%d,%d) (%d,%d) \n(vx,vy) = (%d,%d)", map_main_x, map_main_y,    grid(g_link.x), grid(g_link.y),  g_vp.x, g_vp.y);
	pal_bg_mem[0]		= REG_VCOUNT;
	return 0;
}

// EOF
