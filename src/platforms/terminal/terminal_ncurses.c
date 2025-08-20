#include "terminal_ncurses.h"
#include "terminal_utils.h"

#include <string.h>

static WINDOW *main_window = NULL;
static WINDOW *debug_window = NULL;
static WINDOW *audiovis_window = NULL;

#define WINDOW_HEIGHT (24)
#define WINDOW_WIDTH (96)

#define DEBUG_WINDOW_HEIGHT (24)
#define DEBUG_WINDOW_WIDTH (128)

#define AUDIOVIS_WINDOW_HEIGHT (32)
#define AUDIOVIS_WINDOW_WIDTH (64)

#define COLOR_PAIR_BG_BLACK (0)
#define COLOR_PAIR_BG_RED (1)
#define COLOR_PAIR_BG_GREEN (2)
#define COLOR_PAIR_BG_YELLOW (3)
#define COLOR_PAIR_BG_BLUE (4)
#define COLOR_PAIR_BG_MAGENTA (5)
#define COLOR_PAIR_BG_CYAN (6)
#define COLOR_PAIR_BG_WHITE (7)

#define DEBUG_RING_CAPACITY (10)
#define DEBUG_MESSAGE_MAX_LEN (128)

typedef struct DebugRing
{
    uint8_t head;
    uint8_t len;
    char debug_messages[DEBUG_RING_CAPACITY][DEBUG_MESSAGE_MAX_LEN];
} DebugRing_t;

static uint8_t gfx_buffer[WINDOW_HEIGHT][WINDOW_WIDTH] = { { COLOR_PAIR_BG_YELLOW } };
static DebugRing_t debug_ring = {0};
static bool debug_is_break = false;

/// TODO: uhhh make this ... better?
static uint8_t rgb_to_color_pair(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t sum = r + g + b;

    if (sum < 32) return COLOR_PAIR_BG_BLACK;
    if (sum > 720) return COLOR_PAIR_BG_WHITE;

    uint16_t current_score = 0;
    uint16_t high_score = 0; 
    uint8_t winner = 0;
    
    high_score = r;
    winner = 1;

    if (g > high_score)
    {
        high_score = g;
        winner = 2;
    }

    if (b > high_score)
    {
        high_score = b;
        winner = 4;
    }

    current_score = (r + g) / 2;

    if (current_score >= high_score)
    {
        high_score = current_score;
        winner = 3;
    }

    current_score = (r + b) / 2;

    if (current_score >= high_score)
    {
        high_score = current_score;
        winner = 5;
    }

    current_score = (g + b) / 2;

    if (current_score >= high_score)
    {
        //high_score = pixel[1] + pixel[2];
        winner = 6;
    }

    return winner;
}

static void debug_refresh_window(void)
{
    werase(debug_window);

    wattron(debug_window, COLOR_PAIR(COLOR_PAIR_BG_RED));
    mvwprintw(debug_window, 0, 0, "%s", debug_is_break ? "BREAK - c to continue" : "DEBUG");
    wattroff(debug_window, COLOR_PAIR(COLOR_PAIR_BG_RED));

    uint8_t idx = debug_ring.head;

    for (uint8_t i = 0; i < debug_ring.len; i++)
    {
        wattron(debug_window, COLOR_PAIR(COLOR_PAIR_BG_BLACK));
        mvwprintw(debug_window, i, 8, "[%u]%u: %s", idx, i, debug_ring.debug_messages[idx]);
        wattroff(debug_window, COLOR_PAIR(COLOR_PAIR_BG_RED));

        idx++;
        if (idx >= DEBUG_RING_CAPACITY) idx = 0;
    }

    wrefresh(debug_window);
}

char input_read(void)
{
    char c = wgetch(main_window);
    return c;
}

void gfx_clear_buffer(void)
{
    memset(gfx_buffer, COLOR_PAIR_BG_BLACK, sizeof(gfx_buffer));
}

void gfx_draw_texture(TextureRGB_t *texture, int start_x, int start_y)
{
    uint8_t color = COLOR_PAIR_BG_BLACK;

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

            uint16_t pixel_idx = 3 * (texture_x + (texture_y * texture->width));

            color = rgb_to_color_pair(texture->pixels[pixel_idx], texture->pixels[pixel_idx+1], texture->pixels[pixel_idx+2]);
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
    uint8_t idx = debug_ring.head + debug_ring.len % DEBUG_RING_CAPACITY;
    snprintf(debug_ring.debug_messages[idx], DEBUG_MESSAGE_MAX_LEN, "%s", message);

    if (debug_ring.len < DEBUG_RING_CAPACITY)
    {
        debug_ring.len++;
    }
    else
    {
        debug_ring.head++;
        if (debug_ring.head >= DEBUG_RING_CAPACITY) debug_ring.head = 0;
    }

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
    static const int mid_row = AUDIOVIS_WINDOW_HEIGHT/4;

    werase(audiovis_window);

    uint16_t frames_per_column = (AUDIO_PLATFORM_BUFFER_LENGTH / 2) / AUDIOVIS_WINDOW_WIDTH;
    bool silence = false;

    for (uint16_t i = 0; i < AUDIOVIS_WINDOW_WIDTH; i++)
    {
        float frame_sums_stereo[2] = { 0.0f, 0.0f };

        for (uint16_t j = 0; j < frames_per_column; j++)
        {
            uint16_t idx = (audio_buffer->head + (i * frames_per_column) + j);

            if (idx >= AUDIO_PLATFORM_BUFFER_LENGTH) idx -= AUDIO_PLATFORM_BUFFER_LENGTH;

            int mid_color = idx == audio_buffer->head ? COLOR_PAIR_BG_CYAN : COLOR_PAIR_BG_BLUE;

            wattron(audiovis_window, COLOR_PAIR(mid_color));
            mvwaddch(audiovis_window, mid_row, i, ' ');
            mvwaddch(audiovis_window, mid_row*3, i, ' ');
            wattroff(audiovis_window, COLOR_PAIR(mid_color));

            if (idx == audio_buffer->tail)
            {
                silence = true;
                break;
            }

            if (idx % 2 == 0)
            {
                frame_sums_stereo[0] += audio_buffer->buffer[idx];
            }
            else
            {
                frame_sums_stereo[1] += audio_buffer->buffer[idx];
            }
        }

        if (silence)
        {
            continue;
        }

        for (int8_t j = 0; j < 2; j++)
        {
            float value = frame_sums_stereo[j] / (frames_per_column * 0.5f);
            float dir = value > 0.0f ? 1.0f : -1.0f;
            
            int max_val_abs = mid_row * value * dir;
            int color_pair = value > 0.0f ? COLOR_PAIR_BG_GREEN : COLOR_PAIR_BG_RED;

            wattron(audiovis_window, COLOR_PAIR(color_pair));

            for (int k = 1; k <= max_val_abs; k++)
            {
                mvwaddch(audiovis_window, (mid_row * (1 + (2*j)))+(k*dir*-1), i, ' ');
            }

            wattroff(audiovis_window, COLOR_PAIR(color_pair));
        }

    }

    wrefresh(audiovis_window);
}

void gfx_init(void)
{
    initscr();

    noecho();
    cbreak();

    debug_ring.head = 0;
    debug_ring.len = 0;

    main_window = newwin(WINDOW_HEIGHT, WINDOW_WIDTH, 0, 0);
    debug_window = newwin(DEBUG_WINDOW_HEIGHT, DEBUG_WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    audiovis_window = newwin(AUDIOVIS_WINDOW_HEIGHT, AUDIOVIS_WINDOW_WIDTH, 0, WINDOW_WIDTH);

    nodelay(main_window, true);
    nodelay(debug_window, true);

    start_color();

    init_pair(COLOR_PAIR_BG_BLACK, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_PAIR_BG_RED, COLOR_WHITE, COLOR_RED);
    init_pair(COLOR_PAIR_BG_GREEN, COLOR_WHITE, COLOR_GREEN);
    init_pair(COLOR_PAIR_BG_YELLOW, COLOR_WHITE, COLOR_YELLOW);
    init_pair(COLOR_PAIR_BG_MAGENTA, COLOR_WHITE, COLOR_MAGENTA);
    init_pair(COLOR_PAIR_BG_CYAN, COLOR_WHITE, COLOR_CYAN);
    init_pair(COLOR_PAIR_BG_BLUE, COLOR_WHITE, COLOR_BLUE);
    init_pair(COLOR_PAIR_BG_WHITE, COLOR_WHITE, COLOR_WHITE);

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
