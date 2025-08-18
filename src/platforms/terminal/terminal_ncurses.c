#include "terminal_ncurses.h"

#include <string.h>

static uint8_t mock_texture_count = 0;
static uint8_t mock_sprite_count = 0;

static MockTexture_t mock_texture_storage[MOCK_TEXTURE_MAX_COUNT] = {0};
static MockSprite_t mock_sprite_buffer[MOCK_SPRITE_MAX_COUNT] = {0};

static WINDOW *main_window = NULL;

#define COLOR_PAIR_BLACK (1)
#define COLOR_PAIR_WHITE (2)
#define COLOR_PAIR_RED (3)
#define COLOR_PAIR_GREEN (4)
#define COLOR_PAIR_BLUE (5)
#define COLOR_PAIR_YELLOW (6)

static uint8_t gfx_buffer[WINDOW_HEIGHT][WINDOW_WIDTH] = { COLOR_PAIR_YELLOW };

/// TODO: uhhh make this ... better?
static uint8_t rbg_to_color_pair(uint8_t *pixel)
{
    uint16_t sum = pixel[0] + pixel[1] + pixel[2];

    if (sum < 64) return COLOR_PAIR_BLACK;
    if (sum > 640) return COLOR_PAIR_WHITE;

    if (pixel[0] > pixel[1] + pixel[2]) return COLOR_PAIR_RED;
    if (pixel[1] > pixel[0] + pixel[2]) return COLOR_PAIR_GREEN;
    if (pixel[2] > pixel[0] + pixel[1]) return COLOR_PAIR_BLUE;

    return COLOR_PAIR_YELLOW;
}

volatile char input_read(void)
{
    //printf("input read:\n");
    char c = wgetch(main_window);
    return c;
}

void gfx_clear_buffer(void)
{
    memset(gfx_buffer, COLOR_PAIR_BLACK, sizeof(gfx_buffer));
}

void gfx_draw_texture(TextureRGB_t *texture, int x, int y)
{
    uint8_t color = COLOR_PAIR_BLACK;

    for (int ypos = 0; ypos < texture->height; ypos++)
    {
        for (int xpos = 0; xpos < texture->width; xpos++)
        {
            color = rbg_to_color_pair(texture->pixels+xpos+(texture->width * ypos));
            gfx_buffer[y+ypos][x+xpos] = color;
        }
    }
}

void gfx_sync_buffer(void)
{
    for (int y = 0; y < WINDOW_HEIGHT; y++)
    {
        for(int x = 0; x < WINDOW_WIDTH; x++)
        {
            wattron(main_window, COLOR_PAIR(gfx_buffer[y][x]));
            mvwaddch(main_window, y, x, ' ');
            wattroff(main_window, COLOR_PAIR(gfx_buffer[y][x]));
        }
    }

    wrefresh(main_window);
}

void gfx_init(void)
{
    initscr();

    nodelay(main_window, true);
    noecho();
    cbreak();

    main_window = newwin(WINDOW_HEIGHT, WINDOW_WIDTH, 0, 0);

    start_color();

    init_pair(COLOR_PAIR_BLACK, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_PAIR_WHITE, COLOR_WHITE, COLOR_WHITE);
    init_pair(COLOR_PAIR_RED, COLOR_WHITE, COLOR_RED);
    init_pair(COLOR_PAIR_GREEN, COLOR_WHITE, COLOR_GREEN);
    init_pair(COLOR_PAIR_BLUE, COLOR_WHITE, COLOR_BLUE);
    init_pair(COLOR_PAIR_YELLOW, COLOR_WHITE, COLOR_YELLOW);

    wrefresh(main_window);
}

void gfx_deinit(void)
{
    if (main_window != NULL)
    {
        werase(main_window);
        delwin(main_window);
    }

    endwin();
}
