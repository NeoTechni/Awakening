#include <stdlib.h>
#include <stdio.h>
#include <tonc.h>
#include <string.h>

#include "api.h"
#include "bigmap.h"
#include "pallets.h"

//[BEGIN INCLUDES]
#include "firstroom_map.h"
#include "indoors_tileset.h"
#include "outdoors_tileset.h"
#include "first_scene.h"
#include "village_map.h"
#include "village_scene.h"
//[END INCLUDES]

#define IS_GBA 				0//set to 1 for production!!!

char unsigned *collisiondata;
bool handleevents;
bool link_moved;

#define occupiedLen 		65536
//bool occupied[occupiedLen];//redo as less u32s

short unsigned *colorpallet_src;
u32 colorpalletLen;

short unsigned *tiledata;
u32 tilesetLen;

short unsigned *mapdata;
int map_main_width;
int map_main_height;

u32 circle_radius;
u32 link_x;
u32 link_y;

u32 current_function 			= 0;
u32 current_stage 				= 0;
u32 waiting_for 				= 0;

const struct events *eventdata;//static const struct LogInterfaceDescriptor *LOG_INTERFACE 

// Blend
#define BLEND_TOP_BG0			(1<<0)			// REG_BLDCNT
#define BLEND_TOP_BG1			(1<<1)
#define BLEND_TOP_BG2			(1<<2)
#define BLEND_TOP_BG3			(1<<3)
#define BLEND_TOP_OBJ			(1<<4)
#define BLEND_TOP_BD			(1<<5)
#define BLEND_LOW_BG0			(1<<8)
#define BLEND_LOW_BG1			(1<<9)
#define BLEND_LOW_BG2			(1<<10)
#define BLEND_LOW_BG3			(1<<11)
#define BLEND_LOW_OBJ			(1<<12)
#define BLEND_LOW_BD			(1<<13)
                     
#define BLEND_MODE_OFF			(0<<6)			// REG_BLDALPHA
#define BLEND_MODE_ALPHA		(1<<6)
#define BLEND_MODE_LIGHT		(2<<6)
#define BLEND_MODE_DARK			(3<<6)
#define BLEND_LOW(n)			((n)<<0)
#define BLEND_HIGH(n)			((n)<<8)
#define BLEND_LEVEL(n)			(BLEND_LOW(n) | BLEND_HIGH(n))
#define BLEND_BALANCE(n)		(BLEND_LOW(n) | BLEND_HIGH(16-n))

#define BLEND_DEPTH(n)			((n)<<0)		// REG_BLDY

void clear_screen(){
	m3_fill(CLR_BLACK);
}

u32 trans_mode 				= trans_none;
u32 oldREG_BLDCNT			= 0;
int trans_frame 			= 0;
int trans_speed				= 0;
EWRAM_CODE void FadeSetMode(u32 mode, u32 stage){//stage is 1 to 16, mode is BLEND_MODE_DARK or BLEND_MODE_LIGHT
	REG_WINOUT 				= 63;
	REG_BLDCNT 				= mode == BLEND_MODE_DARK ? 255 : 191;
	if(stage < 1){
		stage				= 1;
	} else if(stage > 16){
		stage 				= 16;
	}
	REG_BLDY 				= stage;
}

bool donewaiting(u32 forwhat){
	if(forwhat == waiting_for){
		print("Done waiting!");
		waiting_for 		= async_none;
		if(eventdata->stage == NULL){
			print("stage EVENT WAS NULL!!!!!");
		} else if(eventdata->stage(current_function, current_stage)){
			current_stage	++;
			return 			true;
		}
	}
	return 					false;
}

bool waitfor(u32 forwhat, u32 functionindex, u32 stageindex){
	waiting_for 			= forwhat;
	current_function		= functionindex;
	current_stage			= stageindex;
	return 					true;
}

//https://www.coranac.com/tonc/text/dma.htm#sec-demo
#define DMA_HDMA    (DMA_ENABLE | DMA_REPEAT | DMA_AT_HBLANK | DMA_DST_RELOAD)
//#define IN_RANGE(x, min, max)	( (x) >= (min) && (x) < (max) ) // From tonc_math.h
//! Create an array of horizontal offsets for a circular window.
/*! The offsets are to be copied to REG_WINxH each HBlank, either by HDMA or HBlank isr. Offsets provided by modified Bresenham's circle routine (of course); the clipping code is not optional.
*   \param winh Pointer to array to receive the offsets.
*   \param x0   X-coord of circle origin.
*   \param y0   Y-coord of circle origin.
*   \param rr   Circle radius.
*/
u16 g_winh[SCREEN_HEIGHT+1];// The source array
void win_circle(u16 winh[], int x0, int y0, int rr){
    int x=0, y= rr, d= 1-rr;
    u32 tmp;
    memset16(winh, 0, SCREEN_HEIGHT+1);// clear the whole array first

    while(y >= x) {// Side octs
        tmp  							= clamp(x0+y, 0, SCREEN_WIDTH);
        tmp 							+= clamp(x0-y, 0, SCREEN_WIDTH)<<8;
        
        if(IN_RANGE(y0-x, 0, SCREEN_HEIGHT)){
            winh[y0-x]					= tmp;// o4, o7
		}
        if(IN_RANGE(y0+x, 0, SCREEN_HEIGHT)){
            winh[y0+x]					= tmp;// o0, o3
		}

        if(d >= 0){// Change in y: top/bottom octs
            tmp  						= clamp(x0+x, 0, SCREEN_WIDTH);
            tmp 						+= clamp(x0-x, 0, SCREEN_WIDTH)<<8;            
            if(IN_RANGE(y0-y, 0, SCREEN_HEIGHT)){
                winh[y0-y]				= tmp;// o5, o6
			}
            if(IN_RANGE(y0+y, 0, SCREEN_HEIGHT)){
                winh[y0+y]				= tmp;// o1, o2
			}
            d 							-= 2*(--y);
        }
        d 								+= 2*(x++)+3;
    }
    winh[SCREEN_HEIGHT]					= winh[0];
}

void clearmap(u32 charblock, u32 screenblock){//SBB_0
	pal_bg_bank[15][15]					= RGB15( 0, 0, 0);
	const TILE tile 					= {{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}};//8 words of 4 bytes = 32 bytes
	tile_mem[charblock][511]			= tile;
	//se_fill(se_mem[screenblock], 511);
}

void transition(u32 new_trans_mode){
	if (new_trans_mode == trans_none){//draw current transition
		if(trans_mode > trans_none){
			int limit 					= 0;
			int temp 					= 0;
			int speed 					= trans_speed;
			trans_frame 				+= speed;
			
			switch(trans_mode){
				case trans_wipe_left:
					print("wipe left");
					limit 				= SCREEN_WIDTH;
					temp 				= SCREEN_WIDTH - trans_frame;
					m3_rect(temp, 0, temp + speed, SCREEN_HEIGHT, CLR_WHITE);
					break;
				case trans_wipe_right:
					print("wipe left");
					limit 				= SCREEN_WIDTH;
					temp 				= trans_frame - speed;
					m3_rect(temp, 0, trans_frame, SCREEN_HEIGHT, CLR_BLACK);
					break;
				
				case trans_wipe_up:
					print("wipe left");
					limit 				= SCREEN_HEIGHT;
					temp 				= SCREEN_HEIGHT - trans_frame;
					m3_rect(0, temp, SCREEN_WIDTH, temp + speed, CLR_BLACK);
					break;
				case trans_wipe_down:
					print("wipe left");
					limit 				= SCREEN_WIDTH;
					temp 				= trans_frame - speed;
					m3_rect(0, temp, SCREEN_WIDTH, trans_frame, CLR_BLACK);
					break;
					
				case trans_fade_to_black: case trans_fade_to_white:			
					limit 				= (trans_frame >> 4) + 1;
					FadeSetMode(trans_mode == trans_fade_to_black ? BLEND_MODE_DARK : BLEND_MODE_LIGHT, limit);
					limit 				= 239;
					break;
					
				case trans_fade_from_black: case trans_fade_from_white:	
					limit 				= 239;
					FadeSetMode(trans_mode == trans_fade_from_black ? BLEND_MODE_DARK : BLEND_MODE_LIGHT, 16 - (trans_frame >> 4));
					break;
					
				case trans_mozaic:
					print("mozaic");
					limit 				= 255;
					temp 				= trans_frame >> 4;
					REG_MOSAIC 			= MOS_BUILD(temp, temp, temp, temp);
					break;
				
				case trans_close_circle: case trans_open_circle:
					limit 				= 255;
					if(trans_mode == trans_close_circle){
						circle_radius	-= 1;
						if(circle_radius == 0){
							trans_frame	= limit + 1;
						}
					} else {
						circle_radius	+= 1;
						if(circle_radius > SCREEN_WIDTH * 0.5){
							trans_frame	= limit + 1;
						}
					}
					win_circle(g_winh, link_x, link_y, circle_radius);// Fill circle array
					DMA_TRANSFER(&REG_WIN0H, &g_winh[1], 1, 3, DMA_HDMA);// Init win-circle HDMA
					break;
				
				default:
					print("Unsupported transition!{");
			}
			
			if(trans_frame > limit){//finished!
				trans_frame 			= 0;
				switch(trans_mode){
					case trans_mozaic:
						REG_MOSAIC		= 0;
						REG_BG1CNT		= REG_BG1CNT - BG_MOSAIC;
						break;
					case trans_close_circle: case trans_open_circle:
						REG_DISPCNT		= DCNT_MODE0 | DCNT_BG0 | DCNT_BG1 | DCNT_OBJ | DCNT_OBJ_1D;
						//no break!
					case trans_fade_to_black: case trans_fade_to_white: case trans_fade_from_black: case trans_fade_from_white:
						REG_BLDCNT 		= oldREG_BLDCNT;
						REG_BLDY 		= 0;
						break;
				}				
				if(donewaiting(async_transition)){
					new_trans_mode		= trans_none;
					switch(trans_mode){
						case trans_fade_to_black: 	new_trans_mode = trans_fade_from_black; break;
						case trans_fade_to_white: 	new_trans_mode = trans_fade_from_white; break;
						case trans_fade_from_black: new_trans_mode = trans_fade_to_black; break;
						case trans_fade_from_white: new_trans_mode = trans_fade_to_white; break;
						case trans_close_circle: 	new_trans_mode = trans_open_circle; break;
						case trans_open_circle: 	new_trans_mode = trans_close_circle; break;
					}
					if(new_trans_mode != trans_none){
						transition(new_trans_mode);
					}
				} else {
					trans_mode 			= new_trans_mode;
				}
			}
		}
	} else if(trans_frame == 0) {//initialization!	
		trans_mode 						= new_trans_mode;
		trans_speed						= 8;
		oldREG_BLDCNT					= REG_BLDCNT;
		print("Begin transition");
		switch(trans_mode){
			case trans_mozaic:
				REG_BG1CNT				= REG_BG1CNT | BG_MOSAIC;
				trans_speed				= 1;
				break;
			case trans_fade_to_black: case trans_fade_to_white: case trans_fade_from_black: case trans_fade_from_white:
				trans_speed				= 2;
				break;
			case trans_open_circle: case trans_close_circle:
				trans_speed				= 1;
				circle_radius			= 0;
				if(trans_mode == trans_close_circle){
					circle_radius		= SCREEN_WIDTH * 0.5;
				}
				link_x					= 120;
				link_y 					= 120;
			
				logval("Circle start radius: %d", circle_radius);
				REG_BG2CNT				= BG_CBB(3) | BG_SBB(29);
				clearmap(BG_CBB(3), BG_SBB(29));
	
				// Init window
				REG_WIN0H				= SCREEN_WIDTH;
				REG_WIN0V				= SCREEN_HEIGHT;

				// Enable stuff
				REG_DISPCNT				= DCNT_MODE0 | DCNT_BG0 | DCNT_BG1 | DCNT_OBJ | DCNT_OBJ_1D | DCNT_WIN0 | DCNT_BG2;//DCNT_BG2
				REG_WININ				= WIN_BUILD(DCNT_BG0 | WIN_BG1 | DCNT_OBJ | DCNT_OBJ_1D, 0);//what is inside the circle (the map)
				REG_WINOUT				= WIN_BUILD(WIN_BG2, 0);//what is outside the circle (darkness)
			
				break;
		}
	}
}

bool select_pallet(const char *name){
	colorpalletLen				= 0;
	/*[BEGIN TILESET TEMPLATE]
	if(strcmp(name, "[title]") == 0){
		colorpallet_src		= (short unsigned *) [title]Pal;
		colorpalletLen		= [title]PalLen;
		print("DID LOAD PALLET: [title]");
		return				true;
	}
	[END TILESET TEMPLATE]*/
	
	//[BEGIN INDOORS PALLET]
	if(strcmp(name, "indoors") == 0){
		colorpallet_src		= (short unsigned *) indoorsPal;
		colorpalletLen		= indoorsPalLen;
		print("DID LOAD PALLET: indoors");
		return				true;
	}
	
//[END INDOORS PALLET]
	
//[BEGIN OUTDOORS TILESET]

	if(strcmp(name, "outdoors") == 0){
		colorpallet_src		= (short unsigned *) outdoorsPal;
		colorpalletLen		= outdoorsPalLen;
		print("DID LOAD PALLET: outdoors");
		return				true;
	}
	
//[END OUTDOORS TILESET]
//[END TILESET CASE]
	return 					false;
}

bool select_tileset(const char *name){
	tilesetLen				= 0;
	/*[BEGIN TILESET TEMPLATE]
	if(strcmp(name, "[title]") == 0){
		collisiondata		= (char unsigned *) [title]Collision;
		tiledata			= (short unsigned *) [title]Tiles;
		tilesetLen 			= [title]TilesLen;
		logval("DID LOAD TILESET: [title] %d", tilesetLen);
		return				true;
	}
	[END TILESET TEMPLATE]*/
	
	//[BEGIN INDOORS TILESET]
	if(strcmp(name, "indoors") == 0){
		collisiondata		= (char unsigned *) indoorsCollision;
		tiledata			= (short unsigned *) indoorsTiles;
		tilesetLen 			= indoorsTilesLen;
		logval("DID LOAD TILESET: indoors %d", tilesetLen);
		select_pallet("indoors");
		return				true;
	}
	//[END INDOORS TILESET]
	
//[BEGIN OUTDOORS TILESET]

	if(strcmp(name, "outdoors") == 0){
		collisiondata		= (char unsigned *) outdoorsCollision;
		tiledata			= (short unsigned *) outdoorsTiles;
		tilesetLen 			= outdoorsTilesLen;
		logval("DID LOAD TILESET: outdoors %d", tilesetLen);
		select_pallet("outdoors");
		return				true;
	}
	
//[END OUTDOORS TILESET]
//[END TILESET CASE]
	return 					false;
}

bool select_map(const char *name){
	map_main_width			= 0;
	map_main_height			= 0;
	/*[BEGIN MAP TEMPLATE]
	if(strcmp(name, "[title]") == 0){
		mapdata 			= (short unsigned *) [title]Map;
		map_main_width		= [title]MapWidth;
		map_main_height 	= [title]MapHeight;
		print("DID LOAD MAP: [title]");
		select_tileset("[tileset]");
		return				true;
	}
	[END MAP TEMPLATE]*/


	//[BEGIN FIRSTROOM MAP]
	if(strcmp(name, "firstroom") == 0){
		mapdata 			= (short unsigned *) firstroomMap;
		map_main_width		= firstroomMapWidth;
		map_main_height		= firstroomMapHeight;
		print("DID LOAD MAP: firstroom");
		select_tileset("indoors");
		return				true;
	}
	//[END FIRSTROOM MAP]
	
//[BEGIN VILLAGE MAP]

	if(strcmp(name, "village") == 0){
		mapdata 			= (short unsigned *) villageMap;
		map_main_width		= villageMapWidth;
		map_main_height 	= villageMapHeight;
		print("DID LOAD MAP: village");
		select_tileset("outdoors");
		return				true;
	}
	
//[END VILLAGE MAP]
//[END MAP CASE]
	return 					false;
}

bool select_scene(const char *scenename){
	bool ret = false;
	/*[BEGIN SCENE TEMPLATE]
	if(strcmp(name, "[title]") == 0){
		print("DID LOAD SCENE: [title]");
		eventdata 			= &[title]_events;
		ret 				= true;
	}
	[END SCENE TEMPLATE]*/
	
	//[BEGIN FIRST SCENE]
	if(strcmp(scenename, "first") == 0){
		print("DID LOAD SCENE: first");
		eventdata			= &first_events;
		ret 				= true;
	}
	//[BEGIN FIRST SCENE]
	
	//[BEGIN VILLAGE SCENE]
	if(strcmp(scenename, "village") == 0){
		print("DID LOAD SCENE: village");
		eventdata			= &village_events;
		ret 				= true;
	}
	//[BEGIN VILLAGE SCENE]
	
//[END SCENE CASE]
	if(ret){
		if(eventdata->load == NULL){
			print("LOAD EVENT WAS NULL!!!!!");
		} else {
			eventdata->load();
		}
	} else {
		print("Failed to select scene!");
	}
	return 					ret;
}

void move_link(uint x, uint y){//was * 10
	link_moved 				= true;
	link_x					= x * 8;
	link_y					= y * 8;
	logval("Moved Link to X: %d", x);
	logval("Moved Link to Y: %d", y);
}

void init_map(){
	init_map_main(colorpallet_src, colorpalletLen, tiledata, tilesetLen, mapdata, map_main_width, map_main_height, link_x, link_y);
}

void load_map(const char *name){
	select_map(name);
	init_map();
}

bool teleport(const char *scenename, uint x, uint y){
	move_link(x, y);
	if(select_scene(scenename)){
		init_map();
		return				true;
	}
	return 					false;
}








void clearoccupied(u32 map_width, u32 map_height){
	//uint i, len;
	//len = map_width * map_height;
	/*for(i = 0; i < len; i++){
		occupied[i] = false;
	}*/
}

int grid(int pos){
	return pos >> 11;
}

enum side {
	side_none = 0, side_north = 1, side_east = 2, side_south = 4, side_west = 8, side_middle = 16, side_full = 32
};

enum event_types {
	tile_bump, tile_enter, tile_leave, tile_a_button, tile_b_button
};

void print(const char *message){
	if(IS_GBA == 0){//from https://github.com/LunarLambda/sdk-seven/blob/main/libutil/src/log.c
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

void logval(char* format, int x){//format = text: %d
	int length = snprintf( NULL, 0, format, x );
	char* str = malloc( length + 1 );
	snprintf( str, length + 1, format, x );
	print(str);
	free(str);
}

bool is_occupied(uint index, uint theside, bool middle, bool full){
	/*if(occupied[index]){
		return 		false;
	}*/
	uint tileid		= mapdata[index];
	bool flipx 		= (tileid & 1024) == 1024;
	bool flipy 		= (tileid & 2048) == 2048;
	tileid 			= tileid & 0x3ff;//1023
	
	/*if(flipx & flipy){
		logval("Found flipped x and y tile: %d", tileid);
	} else if(flipx){
		logval("Found flipped x tile: %d", tileid);
	} else if(flipy){
		logval("Found flipped y tile: %d", tileid);
	} else {
		logval("Found tile: %d", tileid);
	}*/
	
	//logval("checking index %d", index);
	//logval("tile %d", tileid);
	tileid			= collisiondata[tileid];//masks off flip x/y and pallet
	//logval("Collision Data: %d", tileid);
	
	if(full){
		if((tileid & side_full) == side_full){
			//logval("Full detected %d", index);
			return 	true;
		}
	} else if(middle){
		if((tileid & side_middle) == side_middle){
			//logval("Middle detected %d", index);
			return 	true;
		}
	} else if(theside == side_none){
		return 		false;
	}
	if(flipy){
		if(theside == side_north){
			theside = side_south;
		} else if(theside == side_south){
			theside = side_north;
		}
	}
	if(flipx){
		if(theside == side_east){
			theside = side_west;
		} else if(theside == side_west){
			theside = side_east;
		}
	}
	
	/*if((tileid & theside) == theside){
		switch(theside){
			case side_north: logval("north detected", index); break;
			case side_east:  logval("east detected", index); break;
			case side_south: logval("south detected", index); break;
			case side_west:  logval("west detected", index); break;
		}
		return true;
	} else {
		switch(theside){
			case side_north: logval("nothing north %d", index); break;
			case side_east:  logval("nothing east %d", index); break;
			case side_south: logval("nothing south %d", index); break;
			case side_west:  logval("nothing west %d", index); break;
		}
		return false;
	}*/
	
	return 			(tileid & theside) == theside;
}

bool is_occupied_col(uint x, uint y, uint height, uint theside, bool middle, bool full){
	uint index 		= (y * map_main_width) + x;
	height			= y + height;
	//logval("Checking column: %d", x);
	for(y = y; y < height; y++){
		//logval("Checking row: %d", y);
		if(is_occupied(index, theside, middle, full)){
			return 	true;
		}	
		index 		+= map_main_width;
	}
	return 			false;
}

bool is_occupied_row(uint x, uint y, uint width, uint theside, bool middle, bool full){
	uint index 		= (y * map_main_width) + x;
	width			= index + width;
	//logval("Checking row: %d", y);
	for(x = index; x < width; x++){
		//logval("Checking column: %d", x);
		if(is_occupied(x, theside, middle, full)){
			return 	true;
		}
	}
	return 			false;
}

bool doevent(uint event, uint x, uint y, uint direction){
	//tile_bump, tile_enter, tile_leave, tile_a_button, tile_b_button	
	switch(event){
		case tile_bump: case tile_a_button:	case tile_b_button:
			if(eventdata->tile_interact == NULL){
				return true;//the event doesn't exist, might as well kill the loop
			} else {
				return eventdata->tile_interact(x, y, event);
			}
			break;
		case tile_enter:
			if(eventdata->tile_enter == NULL){
				return true;//the event doesn't exist, might as well kill the loop
			} else {
				return eventdata->tile_enter(x, y, direction);
			}
			break;
		case tile_leave:
			if(eventdata->tile_leave == NULL){
				return true;//the event doesn't exist, might as well kill the loop
			} else {
				return eventdata->tile_leave(x, y, direction);
			}
			break;
	}
	//logval("x: %d", x);
	//logval("y: %d", y);
	return false;
}

bool doevents(uint oldx, uint oldy, uint newx, uint newy, uint width, uint height, bool didntmove, bool horizontal){
	uint temp, direction;
	if(oldx == newx){//moving on Y axis
		if(oldy < newy){
			direction = side_north;
		} else {
			direction = side_south;
		}
	} else if(oldx < newx){
		direction = side_east;
	} else {//if(oldx > newx){
		direction = side_west;
	}
	if(didntmove){//did not move, so bump against a line of tiles
		if(horizontal){//moving left/right so loop must be vertical
			for(temp = oldy; temp <= oldy + height; temp++){
				if(doevent(tile_bump, newx, temp, direction)){
					return true;
				}
			}
		} else {//moving up/down so loop must be horizontal
			for(temp = oldx; temp <= oldx + width; temp++){
				if(doevent(tile_bump, temp, newy, direction)){
					return true;
				}
			}
		}
	} else if(horizontal){//did move horizontally, so exited oldx and entered newx, so loop must be vertical
		for(temp = newy; temp <= newy + height; temp++){
			if(doevent(tile_enter, newx, temp, direction)){
				return true;
			}
		}
	} else {//did move vertically, so exited oldy and entered newy, so loop must be horizontal
		for(temp = oldx; temp <= oldx + width; temp++){
			if(doevent(tile_enter, temp, newy, direction)){
				return true;
			}
		}
	}
	return false;
}

//x,y,width,height are in tiles for simplicity, and are the dimensions of the object's collision rect
//when partial/full = true, it checks if those collision bits are set, if false then the object can pass through regardless of the state of those bits
uint can_move_x(uint cx, uint cy, uint x, uint y, uint width, uint height, int bywhat, bool partial, bool middle, bool full){
	uint previous	= grid(cx);			//previous grid tile
	uint newcoord	= cx + bywhat;		//what the new X coordinate will be
	uint next		= grid(newcoord);	//new grid tile
	uint tempside	= side_none;		//side of current tile to check
	uint tempside2 	= side_none;		//side of new tile to check
	//logval("STARTING NEW X---------%d", previous);
	if(previous == next){//grid tile will be the same, allow any movement
		//print("within same tile");
		return 		newcoord;//do move
	}
	cy 				= grid(cy);
	y				+= cy;
	previous		+= x;
	//logval("newprev: %d", previous);
	//logval("bywhat: %d", bywhat);
	if(bywhat < 0){//check left
		next 		= previous - 1;
		if(partial){
			tempside = side_west;//check west/left side of current tile
			tempside2 = side_east;//check east/right of next tile
		}
	} else {//check right
		next		= previous + width;
		previous 	= next - 1;
		if(partial){
			tempside = side_east;//check east/right side of current tile
			tempside2 = side_west;//check west/left of next tile
		}
	}
	if(is_occupied_col(previous, y, height, tempside, false, false)){//assume they can have moved into the current tile
		newcoord 	= cx;//don't move
	} else if(is_occupied_col(next, y, height, tempside2, middle, full)){
		newcoord 	= cx;//don't move
	}
	if(handleevents){
		link_moved	= false;
		doevents(previous, cy, next, cy, width, height, newcoord == cx, true);
		if(link_moved){
			logval("Link moved X: %d", link_x);
			return 	link_x << 8;
		}
	}
	return 			newcoord;//do move
}

uint can_move_y(uint cx, uint cy, uint x, uint y, uint width, uint height, int bywhat, bool partial, bool middle, bool full){
	uint previous	= grid(cy);			//previous grid tile
	uint newcoord	= cy + bywhat;		//what the new X coordinate will be
	uint next		= grid(newcoord);	//new grid tile
	uint tempside	= side_none;		//side of current tile to check
	uint tempside2 	= side_none;		//side of new tile to check
	//logval("STARTING NEW Y---------%d", previous);
	if(previous == next){//grid tile will be the same, allow any movement
		//print("within same tile");
		return 		newcoord;//do move
	}
	cx 				= grid(cx);
	x				+= cx;
	previous		+= y;
	//logval("newprev: %d", previous);
	//logval("bywhat: %d", bywhat);
	if(bywhat < 0){//check up
		next 		= previous - 1;
		if(partial){
			tempside = side_north;//check north/up side of current tile
			tempside2 = side_south;//check south/down of next tile
		}
	} else {//check down
		next 		= previous + height;
		previous 	= next - 1;
		if(partial){
			tempside = side_south;//check south/down side of current tile
			tempside2 = side_north;//check westnorth/up of next tile
		}
	}
	if(is_occupied_row(x, previous, height, tempside, false, false)){//assume they can have moved into the current tile
		newcoord = cy;//don't move
	} else if(is_occupied_row(x, next, height, tempside2, middle, full)){
		newcoord = cy;//don't move
	}
	if(handleevents){
		link_moved	= false;
		doevents(cx, previous, cx, next, width, height, newcoord == cx, false);
		if(link_moved){
			logval("Link moved Y: %d", link_y);//link->x>>8
			return 	link_y << 8;
		}
	}
	return 			newcoord;//do move
}
