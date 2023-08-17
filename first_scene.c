#include "api.h"

bool first_load(void);
bool first_tile_enter(u32 x, u32 y, u32 direction);
bool first_stage(u32 eventindex, u32 stageindex);

const struct events first_events = {
    first_load, 
    first_tile_enter, 
    NULL, 
    NULL, 
    NULL, 
    first_stage
};

bool first_load(void){
	print("first_load RAN!!");
	move_link(15, 6);
	load_map("firstroom");
    return false;
}


bool first_tile_enter(u32 x, u32 y, u32 direction){
    if ((y == 16) & ((x == 14) | (x == 15))){
        print("Entered the doorway!");
        transition(trans_close_circle);
        waiting_for = async_transition;
		current_function = 1;
        current_stage = 1;
        return true;
    }
    return false;
}

bool first_stage(u32 eventindex, u32 stageindex){
	logval("Event index: %d", eventindex);
	logval("Event stage: %d", stageindex);
    switch(eventindex){
        case 1: //tile_enter
            switch(stageindex){
                case 1:
                    teleport("village", 16, 17);
                    current_stage = 2;
                    return true;
                    break;
				default:
					print("Unsupported stage");
            }
			current_stage = 0;
            break;
		default:
			print("Unsupported event");
    }
    return false;
}

