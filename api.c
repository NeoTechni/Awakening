#include <stdlib.h>
#include <stdio.h>
#include <tonc.h>
#include <string.h>

#include "api.h"
#include "pallets.h"

//[BEGIN INCLUDES]
#include "firstroom_map.h"
#include "indoors_tileset.h"
#include "firstscene.h"
//[END INCLUDES]

#define IS_GBA 				0//set to 1 for production!!!

char unsigned *collisiondata;
bool handleevents;

#define occupiedLen 		65536
bool occupied[occupiedLen];

short unsigned *colorpallet_src;
u32 colorpalletLen;

short unsigned *tiledata;
u32 tilesetLen;

short unsigned *mapdata;
int map_main_width;
int map_main_height;

u32 link_x;
u32 link_y;

static const struct events *eventdata;//static const struct LogInterfaceDescriptor *LOG_INTERFACE 

bool select_pallet(const char *name){
	colorpalletLen				= 0;
	/*[BEGIN TILESET TEMPLATE]
	if(strcmp(name, "[title]") == 0){
		colorpallet_src		= (short unsigned *) [title]Pal;
		colorpalletLen		= [title]PalLen;
		return				true;
	}
	[END TILESET TEMPLATE]*/
	
	//[BEGIN INDOORS PALLET]
	if(strcmp(name, "indoors") == 0){
		colorpallet_src		= (short unsigned *) indoorsPal;
		colorpalletLen		= indoorsPalLen;
		return				true;
	}
	//[END INDOORS PALLET]
	
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
		return				true;
	}
	[END TILESET TEMPLATE]*/
	
	//[BEGIN INDOORS TILESET]
	if(strcmp(name, "indoors") == 0){
		collisiondata		= (char unsigned *) indoorsCollision;
		tiledata			= (short unsigned *) indoorsTiles;
		tilesetLen 			= indoorsTilesLen;
		select_pallet("indoors");
		return				true;
	}
	//[END INDOORS TILESET]
	
//[END TILESET CASE]
	return 					false;
}

bool select_map(const char *name){
	map_main_width			= 0;
	map_main_height			= 0;
	/*[BEGIN MAP TEMPLATE]
	if(strcmp(name, "[title]") == 0){
		mapdata 			= (short unsigned *) [title];
		map_main_width		= [title]Width;
		map_main_height 	= [title]Width;
		return				true;
	}
	[END MAP TEMPLATE]*/


	//[BEGIN FIRSTROOM MAP]
	if(strcmp(name, "firstroom") == 0){
		mapdata 			= (short unsigned *) firstroomMap;
		map_main_width		= firstroomMapWidth;
		map_main_height		= firstroomMapHeight;
		select_tileset("indoors");
		return				true;
	}
	//[END FIRSTROOM MAP]
	
//[END MAP CASE]
	return 					false;
}

bool teleport(const char *scenename, uint x, uint y){
	bool ret = false;
	if(strcmp(scenename, "firstroom") == 0){
		eventdata			= &firstscene_events;
		ret					= true;
	}
	
	if(ret){
		eventdata->load();
	}
	return 					ret;
}








void clearoccupied(u32 map_width, u32 map_height){
	uint i, len;
	len = map_width * map_height;
	for(i = 0; i < len; i++){
		occupied[i] = false;
	}
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
	if(occupied[index]){
		return 		false;
	}
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

bool doevent(uint event, uint x, uint y){
	//tile_bump, tile_enter, tile_leave, tile_a_button, tile_b_button
	switch(event){
		case tile_bump:  	print("BUMP AGAINST"); break;
		case tile_enter: 	print("ENTER"); break;
		case tile_leave: 	print("LEAVE"); break;
		case tile_a_button: print("A button"); break;
		case tile_b_button: print("B button"); break;
	}
	logval("x: %d", x);
	logval("y: %d", y);
	return false;
}

bool doevents(uint oldx, uint oldy, uint newx, uint newy, uint width, uint height, bool didntmove, bool horizontal){
	uint temp;
	if(didntmove){//did not move, so bump against a line of tiles
		if(horizontal){//moving left/right so loop must be vertical
			for(temp = oldy; temp <= oldy + height; temp++){
				if(doevent(tile_bump, newx, temp)){
					return true;
				}
			}
		} else {//moving up/down so loop must be horizontal
			for(temp = oldx; temp <= oldx + width; temp++){
				if(doevent(tile_bump, temp, newy)){
					return true;
				}
			}
		}
	} else if(horizontal){//did move horizontally, so exited oldx and entered newx, so loop must be vertical
		for(temp = newy; temp <= newy + height; temp++){
			if(doevent(tile_enter, newx, temp)){
				return true;
			}
		}
	} else {//did move vertically, so exited oldy and entered newy, so loop must be horizontal
		for(temp = oldx; temp <= oldx + width; temp++){
			if(doevent(tile_enter, temp, newy)){
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
		doevents(previous, cy, next, cy, width, height, newcoord == cx, true);
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
		doevents(cx, previous, cx, next, width, height, newcoord == cx, false);
	}
	return 			newcoord;//do move
}