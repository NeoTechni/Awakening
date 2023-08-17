#ifndef API_C
#define API_C

#include <tonc.h>
#include <stdbool.h>

//[BEGIN EVENTS]
typedef bool event_load(void);
typedef bool event_tile_enter(u32 x, u32 y, u32 direction);
typedef bool event_tile_leave(u32 x, u32 y, u32 direction);//u32 = unsigned int
typedef bool event_tile_interact(u32 x, u32 y, u32 type);
typedef bool event_unload(void);
typedef bool event_stage(u32 eventindex, u32 stageindex);

typedef struct events{
	event_load 			*load;
	event_tile_enter 	*tile_enter;
	event_tile_leave 	*tile_leave;
	event_tile_interact *tile_interact;
	event_unload 		*unload;
	event_stage 		*stage;
} events;
//[END EVENTS]

enum waitingforwhat {
	async_none				= 0,
	async_transition
};

enum transitions {
	trans_none				= 0,
	
	trans_wipe_left,
	trans_wipe_right,
	trans_wipe_up,
	trans_wipe_down,
	
	trans_uncover_left,
	trans_uncover_right,
	trans_uncover_up,
	trans_uncover_down,
	
	trans_fade_to_black,
	trans_fade_to_white,
	trans_fade_from_black,
	trans_fade_from_white,
	
	trans_mozaic,
	
	trans_open_circle,
	trans_close_circle,
};

void transition(u32 new_trans_mode);
void clear_screen();

bool select_pallet(const char *name);
bool select_tileset(const char *name);
bool select_map(const char *name);
bool select_scene(const char *scenename);
void move_link(uint x, uint y);
void init_map();
void load_map(const char *name);
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

bool waitfor(u32 forwhat, u32 functionindex, u32 stageindex);
extern u32 current_function;
extern u32 current_stage;
extern u32 waiting_for;
#endif