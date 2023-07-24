#ifndef API_C
#define API_C

#include <tonc.h>
#include <stdbool.h>

//[BEGIN EVENTS]
typedef bool event_load(void);
typedef bool event_tile_enter(u32 x, u32 y);
typedef bool event_tile_leave(u32 x, u32 y);//u32 = unsigned int
typedef bool event_tile_interact(u32 x, u32 y, u32 type);
typedef bool event_unload(void);
typedef uint event_stage(u32 eventindex, u32 stageindex);

typedef struct events{
	event_load 			*load;
	event_tile_enter 	*tile_enter;
	event_tile_leave 	*tile_leave;
	event_tile_interact *tile_interact;
	event_unload 		*unload;
	event_stage 		*stage;
} events;
//[END EVENTS]

bool select_pallet(const char *name);
bool select_tileset(const char *name);
bool select_map(const char *name);
bool select_scene(const char *scenename);
bool teleport(const char *scenename, uint x, uint y);

extern short unsigned *mapdata;
extern short unsigned *tiledata;
extern char unsigned *collisiondata;
extern bool handleevents;
extern int map_main_width, map_main_height;

void clearoccupied(u32 map_width, u32 map_height);
void logval(char* format, int x);
void print(const char *message);

int grid(int pos);

uint can_move_x(uint cx, uint cy, uint x, uint y, uint width, uint height, int bywhat, bool partial, bool middle, bool full);
uint can_move_y(uint cx, uint cy, uint x, uint y, uint width, uint height, int bywhat, bool partial, bool middle, bool full);

extern u32 current_function;
extern u32 current_line;
#endif