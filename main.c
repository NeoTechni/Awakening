/*
	todo:
	- collision detection for object regions, each frame can have a region
	- animations and pallet swapping
	- UI needs an event editor
		- teleport events write directly to the stage function queue for simplicity
	- API to edit map at runtime
	- sidescroller mode
	- inventory grid mode
	- menu mode
	- font editor
*/

//"C:\devkitPro\visualboyadvance-m.exe" "E:\VB6\GBA\Projects\Awakening\Awakening.gba"

#include <stdio.h>
#include <tonc.h>
#include <stdbool.h>
#include <strings.h>

#include "api.h"

#include "bigmap.h"
#include "images.h"
#include "kakariko.h"

#include "pallets.h"
#include "indoors_tileset.h"
#include "firstroom_map.h"

//define SCREEN_WIDTH   	240
//define SCREEN_HEIGHT  	160

/*
typedef struct { u32 data[8];  } TILE, TILE4;	// tile 8x8@4bpp: 32bytes; 8 ints
typedef struct { u32 data[16]; } TILE8;			// d-tile: double-sized tile (8bpp)
typedef TILE  CHARBLOCK[512];					// tile block: 32x16 tiles, 16x16 d-tiles
typedef TILE8 CHARBLOCK8[256];
#define tile_mem  ( (CHARBLOCK*)0x06000000)
#define tile8_mem ((CHARBLOCK8*)0x06000000)
*/

//In code somewhere
//TILE *ptr= &tile_mem[4][12];   // block 4 (== lower object block), tile 12

// Copy a tile from data to sprite-mem, tile 12
//tile_mem[4][12] = *(TILE*)spriteData;

/*
#define IS_GBA 				0//set to 1 for production!!!

static void print(const char *message){
	if(IS_GBA == 0){//from https://github.com/LunarLambda/sdk-seven/blob/main/libutil/src/log.c#L210
		asm volatile(
			"mov r0, %0;"
			"swi 0xff;"
			:
			: "r" (message)
			: "r0"
		);

		asm volatile(
			"mov r0, %0;"
			"swi 0xff;"
			:
			: "r" ("\n")
			: "r0"
		);
	}
}
*/

//! Get the screen entry index for a tile-coord pair - And yes, the div and mods will be converted by the compiler
uint se_index(uint tx, uint ty, uint pitch){	
    uint sbb= (ty/32)*(pitch/32) + (tx/32);
    return sbb*1024 + (ty%32)*32 + tx%32;
}

int mode = -1;

void change_mode(int newmode){
	//switch(mode){//exiting this mode
	//	case 0:
			clear_screen();
	//		break;
	//}//strcat
	switch(newmode){//entering this mode
		case 0: 
			draw_image("intro1", -1, -1); 
			break;
		case 1://          *colorpallet, colorpalletLen, tileset,       tilesetLen,      *map,         width,             height,          link_x, link_y) {
			//init_map_main(kakarikoPal, kakarikoPalLen, kakarikoTiles, 0,               kakarikoMap,  128,               128,                120, 8);
			REG_BG1CNT |= 128;
			print("loading scene: firstscene");
			select_scene("first");
			//init_map_main(indoorsPal,    indoorsPalLen,  indoorsTiles,  indoorsTilesLen, firstroomMap, firstroomMapWidth, firstroomMapHeight, 120, 50);
			//collisiondata	= (char unsigned *) indoorsCollision;
			break;
	}
	logval("Changed mode: %d", newmode);
	mode = newmode;
}

int main(){	
	irq_init(NULL);
	irq_add(II_VBLANK, NULL);
	
	print("main initialized");
	
	change_mode(0);
	
	while(1){
		VBlankIntrWait();
		key_poll();
		
		if(mode == 0){//change_mode(1);
			/*if(key_is_down(KEY_RIGHT)){
				transition(trans_wipe_right);
			} else if(key_is_down(KEY_LEFT)){
				transition(trans_wipe_left);
			} else if(key_is_down(KEY_DOWN)){
				transition(trans_wipe_down);
			} else if(key_is_down(KEY_UP)){
				transition(trans_wipe_up);
			} else if(key_is_down(KEY_A)){
				change_mode(1);
			} else if(key_is_down(KEY_B)){
				//transition(trans_fade_to_white);
			}*/
			if(key_is_down(KEY_A)){
				change_mode(1);
			}
		} else {
			map_main();
		}
		
		transition(trans_none);
	}
	return 0;
}