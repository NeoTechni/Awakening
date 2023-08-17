#include "api.h"

bool village_load(void);

const struct events village_events = {
    village_load, 
    NULL, 
    NULL, 
    NULL, 
    NULL, 
    NULL
};

bool village_load(void){
    current_function = 0;
    current_stage = 0;
	print("village_load RAN!!");
	load_map("village");
    return false;
}