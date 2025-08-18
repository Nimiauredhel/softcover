#ifndef TERMINAL_NCURSES_H
#define TERMINAL_NCURSES_H

#include <ncurses.h>

#define MOCK_TEXTURE_MAX_COUNT (32)
#define MOCK_SPRITE_MAX_COUNT (32)

#define WINDOW_HEIGHT (32)
#define WINDOW_WIDTH (128)

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

volatile char input_read(void);
void gfx_clear_buffer(void);
int gfx_load_texture(char *name);
void gfx_draw_texture(int idx, int x, int y);
void gfx_sync_buffer(void);
void gfx_init(void);
void gfx_deinit(void);

#endif
