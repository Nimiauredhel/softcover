#include "terminal_ncurses.h"
#include "terminal_utils.h"
#include "terminal_debug.h"
#include "terminal_time.h"

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

static bool ncurses_is_initialized = false;

static uint8_t audiovis_width;
static uint8_t audiovis_height;
static int audiovis_mid_row;

void gfx_refresh_debug_window(DebugRing_t *debug_ring, bool is_break)
{
    werase(debug_window);

    wattron(debug_window, COLOR_PAIR(COLOR_PAIR_BG_RED));
    mvwprintw(debug_window, 0, 0, "%s", is_break ? "BREAK - c to continue" : "DEBUG");
    wattroff(debug_window, COLOR_PAIR(COLOR_PAIR_BG_RED));

    uint8_t idx = debug_ring->head;

    for (uint8_t i = 0; i < debug_ring->len; i++)
    {
        wattron(debug_window, COLOR_PAIR(COLOR_PAIR_BG_BLACK));
        mvwprintw(debug_window, i, 8, "[%u]%u: %s", idx, i, debug_ring->debug_messages[idx]);
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

    float fps = 1000000.0f / time_get_delta_us();
    mvwprintw(main_window, 0, 84, "%.2f fps\n", fps);

    wrefresh(main_window);
}

void gfx_audio_vis(const FloatRing_t *audio_buffer, float volume)
{
    werase(audiovis_window);

    float max_val = 0.0f;
    float min_val = 0.0f;
    float max_abs_val = 0.0f;

    for(uint16_t i = 0; i < audio_buffer->capacity; i++)
    {
        if (audio_buffer->buffer[i] > max_val) max_val = audio_buffer->buffer[i];
        else if (audio_buffer->buffer[i] < min_val) min_val = audio_buffer->buffer[i];
    }

    max_abs_val = max_val;
    if (min_val < 0.0f && min_val * -1.0f > max_val) max_abs_val = min_val * -1.0f;

    float norm = ((float)audiovis_mid_row * 0.5f) / max_abs_val;

    mvwprintw(audiovis_window, 0, 0, "Min: %f Max: %f", min_val, max_val);

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
            float value = norm * (frame_sums_stereo[j] / (frames_per_column * 0.5f));
            float dir = value > 0.0f ? 1.0f : -1.0f;
            
            int max_height_abs = audiovis_mid_row * value * dir;
            int color_pair = silence ? COLOR_PAIR_BG_BLUE
                : value > 0.0f ? COLOR_PAIR_BG_GREEN : COLOR_PAIR_BG_RED;

            wattron(audiovis_window, COLOR_PAIR(color_pair));

            for (int k = 1; k <= max_height_abs; k++)
            {
                mvwaddch(audiovis_window, (audiovis_mid_row * (1 + (2*j)))+(k*dir*-1), i, ' ');
            }

            wattroff(audiovis_window, COLOR_PAIR(color_pair));
        }
    }

    for (uint16_t i = 0; i < (uint16_t)(audiovis_height * (volume * 0.5f)); i++)
    {
        wattron(audiovis_window, COLOR_PAIR(COLOR_PAIR_BG_MAGENTA));
        mvwaddch(audiovis_window, audiovis_height - i, 0, ' ');
        wattroff(audiovis_window, COLOR_PAIR(COLOR_PAIR_BG_MAGENTA));
    }

    wrefresh(audiovis_window);
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

bool gfx_is_initialized(void)
{
    return ncurses_is_initialized;
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

    main_window     = newwin((settings->gfx_buffer_height / 8) * 7, settings->gfx_buffer_width * 0.6f, 0, 0);
    debug_window    = newwin((settings->gfx_buffer_height / 8) * 1, settings->gfx_buffer_width * 0.6f, ((settings->gfx_buffer_height / 8) * 7), 0);
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
