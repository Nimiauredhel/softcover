#include "terminal_ncurses.h"
#include "terminal_utils.h"

#include <string.h>

static WINDOW *main_window = NULL;
static WINDOW *debug_window = NULL;
static WINDOW *audiovis_window = NULL;

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

static bool ncurses_is_initialized = false;

static bool debug_is_break = false;
static DebugRing_t debug_ring = {0};

static uint8_t audiovis_width;
static uint8_t audiovis_height;
static int audiovis_mid_row;

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

/**
 * @brief Inefficiently translates a given RGB color to an approximate ncurses color pair.
 * Likely has room for improvement, but also extremely low priority.
 */
uint8_t gfx_rgb_to_color_pair(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t sum = r + g + b;

    if (sum == 0) return 128; // fake alpha
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

char input_read(void)
{
    if (ncurses_is_initialized)
    {
        return wgetch(main_window);
    }
    else
    {
        return fgetc(stdin);
    }
}

void input_push_to_buffer(PlatformSettings_t *settings, ByteRing_t *input_buffer)
{
    char c = '~';

    for (uint8_t i = 0; i < 8; i++)
    {
        c = input_read();
        if (c == '~') continue;
        byte_ring_push(input_buffer, (uint8_t *)&c, 1);
    }
}

void gfx_sync_buffer(Texture_t *gfx_buffer)
{
    werase(main_window);

    for (int y = 0; y < gfx_buffer->height; y++)
    {
        for(int x = 0; x < gfx_buffer->width; x++)
        {
            uint8_t val = gfx_buffer->pixels[x + (y * gfx_buffer->width)];

            wattron(main_window, COLOR_PAIR(val));
            mvwaddch(main_window, y, x, ' ');
            wattroff(main_window, COLOR_PAIR(val));
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

    if (ncurses_is_initialized)
    {
        debug_refresh_window();
    }
    else
    {
        printf("DEBUG: %s\n", message);
    }
}

void debug_break(void)
{
    if (ncurses_is_initialized)
    {
        debug_is_break = true;
        debug_refresh_window();
    }

    char c = '~';

    while(c != '\n')
    {
        c = ncurses_is_initialized ? input_read() : fgetc(stdin);
    }

    debug_is_break = false;
}

void gfx_audio_vis(const FloatRing_t *audio_buffer)
{
    werase(audiovis_window);

    uint16_t frames_per_column = (audio_buffer->capacity / 2) / audiovis_width;
    uint16_t count = 0; 
    bool silence = false;

    for (uint16_t i = 0; i < audiovis_width; i++)
    {
        float frame_sums_stereo[2] = { 0.0f, 0.0f };

        for (uint16_t j = 0; j < frames_per_column; j++)
        {
            count = j + (i * frames_per_column); 
            uint16_t idx = (audio_buffer->head + count) % audio_buffer->capacity;

            int mid_color = count == 0 ? COLOR_PAIR_BG_CYAN : COLOR_PAIR_BG_BLUE;

            wattron(audiovis_window,  COLOR_PAIR(mid_color));
            mvwaddch(audiovis_window, audiovis_mid_row,     i, ' ');
            mvwaddch(audiovis_window, audiovis_mid_row * 3, i, ' ');
            wattroff(audiovis_window, COLOR_PAIR(mid_color));

            if (!silence && count >= audio_buffer->length)
            {
                silence = true;
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

        for (int8_t j = 0; j < 2; j++)
        {
            float value = frame_sums_stereo[j] / (frames_per_column * 0.5f);
            float dir = value > 0.0f ? 1.0f : -1.0f;
            
            int max_val_abs = audiovis_mid_row * value * dir;
            int color_pair = silence ? COLOR_PAIR_BG_BLUE
                : value > 0.0f ? COLOR_PAIR_BG_GREEN : COLOR_PAIR_BG_RED;

            wattron(audiovis_window, COLOR_PAIR(color_pair));

            for (int k = 1; k <= max_val_abs; k++)
            {
                mvwaddch(audiovis_window, (audiovis_mid_row * (1 + (2*j)))+(k*dir*-1), i, ' ');
            }

            wattroff(audiovis_window, COLOR_PAIR(color_pair));
        }
    }

    wrefresh(audiovis_window);
}

void debug_init(void)
{
    debug_ring.head = 0;
    debug_ring.len = 0;
}

void input_init(PlatformSettings_t *settings, ByteRing_t **input_buffer_pptr)
{
    /// init input buffer
    *input_buffer_pptr = (ByteRing_t *)malloc(sizeof(ByteRing_t) +
    settings->input_buffer_capacity);
    ByteRing_t *input_buffer = (ByteRing_t *)*input_buffer_pptr;
    memset(input_buffer, 0, sizeof(*input_buffer));
    input_buffer->capacity = settings->input_buffer_capacity;
}

void gfx_init(PlatformSettings_t *settings, Texture_t **gfx_buffer_pptr)
{
    /// init gfx buffer
    *gfx_buffer_pptr = (Texture_t *)malloc(sizeof(Texture_t) +
    (settings->gfx_buffer_width * settings->gfx_buffer_height * settings->gfx_pixel_size_bytes));
    Texture_t *gfx_buffer = (Texture_t *)*gfx_buffer_pptr;
    memset(gfx_buffer, 0, sizeof(*gfx_buffer));
    gfx_buffer->width = settings->gfx_buffer_width;
    gfx_buffer->height = settings->gfx_buffer_height;
    gfx_buffer->pixel_size_bytes = settings->gfx_pixel_size_bytes;

    /// init ncurses

    initscr();
    noecho();
    cbreak();

    audiovis_width = settings->gfx_buffer_width * 0.4f;
    audiovis_height = settings->gfx_buffer_height;
    audiovis_mid_row = audiovis_height / 4;

    main_window     = newwin((settings->gfx_buffer_height / 16) * 11, settings->gfx_buffer_width * 0.6f, 0, 0);
    debug_window    = newwin((settings->gfx_buffer_height / 16) * 5, settings->gfx_buffer_width * 0.6f, ((settings->gfx_buffer_height / 16) * 11), 0);
    audiovis_window = newwin(audiovis_height,                    audiovis_width                   , 0, settings->gfx_buffer_width * 0.6f);

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

    ncurses_is_initialized = true;
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

    ncurses_is_initialized = false;
}
