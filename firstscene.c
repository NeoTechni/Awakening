#include "api.h"
//this is firstscene.c

static event_load firstscene_load;
static event_tile_enter firstscene_tile_enter;

static const struct events firstscene_events = {
    firstscene_load, 
    firstscene_tile_enter, 
    null, 
    null, 
    null, 
    null
}

extern bool firstscene_load(void){
    current_function = 0;
    current_line = 0;
    teleport("firstroom", 15, 6);
    return false;
}

extern bool firstscene_tile_enter(u32 x, u32 y){
    current_function = 1;
    current_line = 0;
    if (y == 16 & (x == 14 | x == 15)){
        print("Entered the doorway!");
        //teleport("village", 16, 17);
    }
    return false;
}