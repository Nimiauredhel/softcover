#include "terminal_ncurses.h"
#include "terminal_utils.h"

#include <string.h>

static WINDOW *main_window = NULL;
static WINDOW *debug_window = NULL;
static WINDOW *audiovis_window = NULL;

#define COLOR_PAIR_BLACK (1)
#define COLOR_PAIR_WHITE (2)
#define COLOR_PAIR_RED (3)
#define COLOR_PAIR_GREEN (4)
#define COLOR_PAIR_BLUE (5)
#define COLOR_PAIR_YELLOW (6)

#define DEBUG_RING_CAPACITY (10)
#define DEBUG_MESSAGE_MAX_LEN (128)

typedef struct DebugRing
{
    uint8_t head;
    uint8_t tail;
    uint8_t len;
    char debug_messages[DEBUG_RING_CAPACITY][DEBUG_MESSAGE_MAX_LEN];
} DebugRing_t;

static uint8_t gfx_buffer[WINDOW_HEIGHT][WINDOW_WIDTH] = { { COLOR_PAIR_YELLOW } };
static DebugRing_t debug_ring = {0};
static bool debug_is_break = false;

/// TODO: uhhh make this ... better?
static uint8_t rgb_to_color_pair(uint8_t *pixel)
{
    uint16_t sum = pixel[0] + pixel[1] + pixel[2];

    if (sum < 64) return COLOR_PAIR_BLACK;
    if (sum > 640) return COLOR_PAIR_WHITE;

    if (pixel[0] > pixel[1] + pixel[2]) return COLOR_PAIR_RED;
    if (pixel[1] > pixel[0] + pixel[2]) return COLOR_PAIR_GREEN;
    if (pixel[2] > pixel[0] + pixel[1]) return COLOR_PAIR_BLUE;

    return COLOR_PAIR_YELLOW;
}

static void debug_refresh_window(void)
{
    werase(debug_window);

    wattron(debug_window, COLOR_PAIR(COLOR_PAIR_RED));
    mvwprintw(debug_window, 0, 0, "%s", debug_is_break ? "BREAK - c to continue" : "DEBUG");
    wattroff(debug_window, COLOR_PAIR(COLOR_PAIR_RED));

    uint8_t idx = debug_ring.head;

    for (uint8_t i = 0; i < debug_ring.len; i++)
    {
        wattron(debug_window, COLOR_PAIR(COLOR_PAIR_BLACK));
        mvwprintw(debug_window, i, 10, "%u: %s", i, debug_ring.debug_messages[idx]);
        wattroff(debug_window, COLOR_PAIR(COLOR_PAIR_RED));

        idx++;
        if (idx >= DEBUG_RING_CAPACITY) idx = 0;
    }

    wrefresh(debug_window);
}

char input_read(void)
{
    //printf("input read:\n");
    char c = wgetch(main_window);
    return c;
}

void gfx_clear_buffer(void)
{
    memset(gfx_buffer, COLOR_PAIR_BLACK, sizeof(gfx_buffer));
}

void gfx_draw_texture(TextureRGB_t *texture, int start_x, int start_y)
{
    uint8_t color = COLOR_PAIR_BLACK;

    int final_x;
    int final_y;

    for (uint16_t texture_y = 0; texture_y < texture->height; texture_y++)
    {
        for (uint16_t texture_x = 0; texture_x < texture->width; texture_x++)
        {
            final_y = start_y + texture_y;
            final_x = start_x + texture_x;

            while(final_y >= WINDOW_HEIGHT) final_y -= WINDOW_HEIGHT;
            while(final_y < 0) final_y += WINDOW_HEIGHT;
            while(final_x >= WINDOW_WIDTH) final_x -= WINDOW_WIDTH;
            while(final_x < 0) final_x += WINDOW_WIDTH;

            color = rgb_to_color_pair(texture->pixels+texture_x+(texture->width * texture_y));
            gfx_buffer[final_y][final_x] = color;
        }
    }
}

void gfx_sync_buffer(void)
{
    werase(main_window);

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

void debug_log(char *message)
{
    snprintf(debug_ring.debug_messages[debug_ring.tail], DEBUG_MESSAGE_MAX_LEN, "%s", message);
    debug_ring.tail++;

    if (debug_ring.tail >= DEBUG_RING_CAPACITY)
    {
        debug_ring.tail = 0;
    }

    if (debug_ring.len < DEBUG_RING_CAPACITY)
    {
        debug_ring.len++;
    }

    if (debug_ring.tail == debug_ring.head) debug_ring.head++;
    if (debug_ring.head >= DEBUG_RING_CAPACITY) debug_ring.head = 0;

    debug_refresh_window();

}

void debug_break(void)
{
    debug_is_break = true;
    debug_refresh_window();

    char c = '~';

    while(c != '\n' && !should_terminate)
    {
        c = input_read();
    }

    debug_is_break = true;
}

void gfx_audio_vis(const AudioBuffer_t *audio_buffer)
{
    static const int8_t mid_row = AUDIOVIS_WINDOW_HEIGHT/2;
    
    float last_value = 0.0f;

    werase(audiovis_window);

    for (uint16_t i = 0; i < audio_buffer->length; i++)
    {
        if (i >= AUDIOVIS_WINDOW_WIDTH) break;

        uint16_t idx = (audio_buffer->head + i) % AUDIO_USER_BUFFER_LENGTH;

        float value = audio_buffer->buffer[idx];
        //float difference = (value - last_value)/2.0f;
        last_value = value;
        int8_t dir = value > 0.0f ? -1 : 1;
        
        uint8_t max_val_abs = (uint8_t)(((float)mid_row)*value);
        int color_pair = value > 0 ? COLOR_PAIR_GREEN : COLOR_PAIR_RED;

        wattron(audiovis_window, COLOR_PAIR(COLOR_PAIR_BLUE));
        mvwaddch(audiovis_window, mid_row, i, ' ');
        wattroff(audiovis_window, COLOR_PAIR(COLOR_PAIR_BLUE));

        wattron(audiovis_window, COLOR_PAIR(color_pair));

        for (int j = 1; j <= max_val_abs; j++)
        {
            mvwaddch(audiovis_window, mid_row+(j*dir), i, ' ');
        }

        wattroff(audiovis_window, COLOR_PAIR(color_pair));
    }

    wrefresh(audiovis_window);
}

void gfx_init(void)
{
    initscr();

    noecho();
    cbreak();

    debug_ring.head = 0;
    debug_ring.tail = 0;
    debug_ring.len = 0;

    main_window = newwin(WINDOW_HEIGHT, WINDOW_WIDTH, 0, 0);
    debug_window = newwin(DEBUG_WINDOW_HEIGHT, DEBUG_WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    audiovis_window = newwin(AUDIOVIS_WINDOW_HEIGHT, AUDIOVIS_WINDOW_WIDTH, 0, WINDOW_WIDTH);

    nodelay(main_window, true);
    nodelay(debug_window, true);

    start_color();

    init_pair(COLOR_PAIR_BLACK, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_PAIR_WHITE, COLOR_WHITE, COLOR_WHITE);
    init_pair(COLOR_PAIR_RED, COLOR_WHITE, COLOR_RED);
    init_pair(COLOR_PAIR_GREEN, COLOR_WHITE, COLOR_GREEN);
    init_pair(COLOR_PAIR_BLUE, COLOR_WHITE, COLOR_BLUE);
    init_pair(COLOR_PAIR_YELLOW, COLOR_WHITE, COLOR_YELLOW);

    system("clear");
    wrefresh(main_window);
    wrefresh(debug_window);
    wrefresh(audiovis_window);
    debug_log("Gfx initialized.");
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
