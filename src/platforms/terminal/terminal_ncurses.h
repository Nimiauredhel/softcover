#ifndef TERMINAL_NCURSES_H
#define TERMINAL_NCURSES_H

#include <ncurses.h>

#include "softcover_platform.h"

#define MOCK_TEXTURE_MAX_COUNT (32)
#define MOCK_SPRITE_MAX_COUNT (32)

#define WINDOW_HEIGHT (24)
#define WINDOW_WIDTH (128)

#define DEBUG_WINDOW_HEIGHT (32)
#define DEBUG_WINDOW_WIDTH (128)

typedef struct MockTexture
{
    char name[32];
} MockTexture_t;

typedef struct MockSprite
{
    int texture_idx;
    int x;
    int y;
} MockSprite_t;

char input_read(void);
void gfx_clear_buffer(void);
void gfx_draw_texture(TextureRGB_t *texture, int x, int y);
void gfx_sync_buffer(void);
void debug_log(char *message);
void debug_break(void);
void gfx_init(void);
void gfx_deinit(void);

#endif
