#include "terminal_ncurses.h"

#include <string.h>

static uint8_t mock_texture_count = 0;
static uint8_t mock_sprite_count = 0;

static MockTexture_t mock_texture_storage[MOCK_TEXTURE_MAX_COUNT] = {0};
static MockSprite_t mock_sprite_buffer[MOCK_SPRITE_MAX_COUNT] = {0};

static WINDOW *main_window = NULL;

volatile char input_read(void)
{
    //printf("input read:\n");
    char c = wgetch(main_window);
    return c;
}

void gfx_clear_buffer(void)
{
    mock_sprite_count = 0;
    //printf("gfx clear buffer.\n");
}

int gfx_load_texture(char *name)
{
    if (mock_texture_count >= MOCK_TEXTURE_MAX_COUNT) return -1;

    strncpy(mock_texture_storage[mock_texture_count].name, name, 32);
    //printf("gfx load texture: %s.\n", name);
    mock_texture_count++;
    return mock_texture_count-1;
}

void gfx_draw_texture(int idx, int x, int y)
{
    if (mock_sprite_count >=MOCK_SPRITE_MAX_COUNT) return;

    //printf("gfx draw texture: idx %d, x %d, y %d.\n", idx, x, y);
    mock_sprite_buffer[mock_sprite_count].texture_idx = idx;
    mock_sprite_buffer[mock_sprite_count].x = x;
    mock_sprite_buffer[mock_sprite_count].y = y;
    mock_sprite_count++;
}

void gfx_init(void)
{
    initscr();

    noecho();
    cbreak();

    main_window = newwin(WINDOW_HEIGHT, WINDOW_WIDTH, 0, 0);

    nodelay(main_window, true);

    werase(main_window);
    wrefresh(main_window);
}

void gfx_sync_buffer(void)
{
    werase(main_window);

    for (int i = 0; i < mock_sprite_count; i++)
    {
        mvwprintw(main_window,
                (WINDOW_HEIGHT/2)+mock_sprite_buffer[i].y,
                (WINDOW_WIDTH/2)+mock_sprite_buffer[i].x,
                "%s [%d,%d]", mock_texture_storage[mock_sprite_buffer[i].texture_idx].name,
                mock_sprite_buffer[i].y,
                mock_sprite_buffer[i].x);
    }

    wrefresh(main_window);
}
