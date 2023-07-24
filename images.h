#pragma once
bool select_image(const char *name);
bool draw_image(const char *name, int x, int y);
void draw_image_raw(const void *src, uint width, uint height, int x, int y);
void clear_screen();