#include "api.h"
//this is firstscene.c

event_load firstscene_load;
event_tile_enter firstscene_tile_enter;

const struct events firstscene_events = {
    firstscene_load, 
    firstscene_tile_enter, 
    NULL, 
    NULL, 
    NULL, 
    NULL
};

bool firstscene_load(void){
    current_function = 0;
    current_line = 0;
	print("firstscene_load RAN!!");
    teleport("firstroom", 15, 6);
    return false;
}

bool firstscene_tile_enter(u32 x, u32 y){
    current_function = 1;
    current_line = 0;
    if ((y == 16) & ((x == 14) | (x == 15))){
        print("Entered the doorway!");
        //teleport("village", 16, 17);
    }
    return false;
}