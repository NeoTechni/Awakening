/* old code
void draw_image(const void *src, uint width, uint height){
	uint size;	
	if (SCREEN_WIDTH == width && SCREEN_HEIGHT == height){
		size = width * height * 2;//2 bytes per pixel
		dma3_cpy(vid_mem, src, size);
	} else {
		uint y;
		u8 *row;
		u16* dest;		
		dest = vid_mem;
		size = width * 2;//2 bytes per pixel
		row = src;
		for(y = 0; y < height; y++){
			dma3_cpy(dest, row, size);
			row += size;
			dest += SCREEN_WIDTH;
		}
	}
}
	
	DO NOT MODIFY MANUALLY
*/


#include <tonc.h>
#include <string.h>

#include "assets.h"
//#include "images.h"

#ifndef IMAGES_C
#define IMAGES_C
	short unsigned *image_src;
	uint image_width					= 0;
	uint image_height					= 0;
#endif /* IMAGES_C */

bool select_image(const char *name){
	image_width							= 0;
	image_height						= 0;
	/*[BEGIN SWITCH TEMPLATE]
	if(strcmp(name, "[title]") == 0){
		image_src 			= (short unsigned *) [title];
		image_width 		= [title]Width;
		image_height 		= [title]Width;
		return				true;
	}
	[END SWITCH TEMPLATE]*/


	//[BEGIN INTRO1]

	if(strcmp(name, "intro1") == 0){
		image_src 			= (short unsigned *) intro1;
		image_width 		= intro1Width;
		image_height 		= intro1Width;
		return				true;
	}
	//[END INTRO1]
//[END SWITCH CASE]
	return 					false;
}

void clear_screen(){
	m3_fill(CLR_BLACK);
}

void draw_image_raw(const void *src, uint width, uint height, int x, int y){
	uint size;
	REG_DISPCNT 					= DCNT_MODE3 | DCNT_BG2;
	if (SCREEN_WIDTH == width && SCREEN_HEIGHT == height){//same size, just DMA the whole thing
		size 						= width * height * 2;//2 bytes per pixel
		dma3_cpy(vid_mem, src, size);
	} else {
		u8 *row 					= (u8 *) src;
		u16* dest 					= vid_mem;
		size 						= width * 2;//2 bytes per pixel
		if(x < 0){//center
			x 						= SCREEN_WIDTH * 0.5 - width * 0.5;
		} 
		if(x > 0){
			dest 					+= x;
		}
		if(y < 0){//center
			y 						= SCREEN_HEIGHT * 0.5 - height * 0.5;
		} 
		if (y > 0){
			dest 					+= SCREEN_WIDTH * y;
		}
		for(y = 0; y < height; y++){
			dma3_cpy(dest, row, size);
			row 					+= size;
			dest 					+= SCREEN_WIDTH;
		}
	}
}

bool draw_image(const char *name, int x, int y){
	if(select_image(name)){
		draw_image_raw(image_src, image_width, image_height, x, y);
		return 				true;
	}
	return 					false;
}
