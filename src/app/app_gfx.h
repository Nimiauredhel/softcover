#ifndef APP_GFX_H
#define APP_GFX_H

#include <stdint.h>
#include <stdbool.h>

#include "common_structs.h"

#define APP_GFX_TILE_WIDTH_PX (8)
#define APP_GFX_TILE_HEIGHT_PX (8)

#define APP_GFX_VIEWPORT_WIDTH_TILES (80)
#define APP_GFX_VIEWPORT_HEIGHT_TILES (80)

extern bool debug_gfx;

void gfx_world_to_screen_coords(int16_t *x_ptr, int16_t *y_ptr);
void gfx_clear_buffer(void);
void gfx_draw_texture(Texture_t *texture, int start_x, int start_y);
void gfx_draw_thing(uint32_t thing_idx);
void gfx_draw_all_entities(void);
void gfx_debug_draw_collider(uint32_t thing_idx, uint32_t draw_val);
void gfx_draw_all_entities_debug(void);

#endif

