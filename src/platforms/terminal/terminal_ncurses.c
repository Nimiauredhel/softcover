#include "terminal_ncurses.h"
#include "terminal_utils.h"
#include "terminal_debug.h"
#include "terminal_time.h"

#include <string.h>

static WINDOW *main_window = NULL;
static WINDOW *debug_window = NULL;

#define COLOR_PAIR_BG_BLACK (0)
#define COLOR_PAIR_BG_RED (1)
#define COLOR_PAIR_BG_GREEN (2)
#define COLOR_PAIR_BG_YELLOW (3)
#define COLOR_PAIR_BG_BLUE (4)
#define COLOR_PAIR_BG_MAGENTA (5)
#define COLOR_PAIR_BG_CYAN (6)
#define COLOR_PAIR_BG_WHITE (7)

static bool ncurses_is_initialized = false;
static GfxDebugMode_t gfx_debug_mode = 0;

static uint8_t main_window_full_width;
static uint8_t main_window_full_height;

static uint8_t main_window_partial_width;
static uint8_t main_window_partial_height;

static uint8_t main_window_current_width;
static uint8_t main_window_current_height;

static uint8_t debug_window_width;
static uint8_t debug_window_height;

static int audiovis_mid_row;

GfxDebugMode_t gfx_get_debug_mode(void)
{
    return gfx_debug_mode;
}

void gfx_toggle_debug_mode(void)
{
    gfx_debug_mode = (gfx_debug_mode + 1) % GFX_DEBUG_MAXVAL;

    switch(gfx_debug_mode)
    {
    case GFX_DEBUG_NONE:
        main_window_current_width =  main_window_full_width;
        main_window_current_height = main_window_full_height;
        wresize(main_window, main_window_current_height, main_window_current_width);
        break;
    case GFX_DEBUG_LOG:
        main_window_current_width =  main_window_partial_width;
        main_window_current_height = main_window_partial_height;
        wresize(main_window, main_window_current_height, main_window_current_width);
        debug_dump_log();
        break;
    case GFX_DEBUG_AUDIO:
    case GFX_DEBUG_MAXVAL:
      break;
    }
}

void gfx_refresh_debug_window(DebugRing_t *debug_ring, bool is_break)
{
    if (gfx_debug_mode != GFX_DEBUG_LOG) return;
    werase(debug_window);

    wattron(debug_window, COLOR_PAIR(COLOR_PAIR_BG_RED));
    mvwprintw(debug_window, 0, 0, "%s", is_break ? "BREAK" : "DEBUG");
    wattroff(debug_window, COLOR_PAIR(COLOR_PAIR_BG_RED));

    int32_t mod = debug_ring->len - debug_window_height;
    if (mod < 0) mod = 0;
    uint16_t idx = (debug_ring->head + mod) % DEBUG_RING_CAPACITY;;

    for (uint16_t i = 1; i < debug_ring->len; i++)
    {
        wattron(debug_window, COLOR_PAIR(COLOR_PAIR_BG_BLACK));
        mvwprintw(debug_window, i, 0, "[%u]%s", idx, debug_ring->debug_messages[idx]);
        wattroff(debug_window, COLOR_PAIR(COLOR_PAIR_BG_RED));

        idx = (idx + 1) % DEBUG_RING_CAPACITY;
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

int input_read(void)
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

void input_push_to_buffer(PlatformSettings_t *settings, UniformRing_t *input_buffer)
{
    int c = '~';

    for (uint8_t i = 0; i < 8; i++)
    {
        c = input_read();
        if (c == '~') continue;
        if (c == KEY_LEFT)
        {
            gfx_toggle_debug_mode();
            continue;
        }
        ring_push(input_buffer, &c, 1, false);
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

void gfx_audio_vis(const UniformRing_t *audio_buffer, const PlatformSettings_t *settings, float volume)
{
    werase(debug_window);

    float max_val = 0.0f;
    float min_val = 0.0f;
    float max_abs_val = 0.0f;

    for(uint32_t i = 0; i < audio_buffer->capacity; i++)
    {
        float temp_val = 0.0f;
        ring_peek(audio_buffer, i, &temp_val, true);
        if (temp_val > max_val) max_val = temp_val;
        else if (temp_val < min_val) min_val = temp_val;
    }

    max_abs_val = max_val;
    if (min_val < 0.0f && min_val * -1.0f > max_val) max_abs_val = min_val * -1.0f;

    float norm = ((float)audiovis_mid_row * 0.5f) / max_abs_val;

    mvwprintw(debug_window, 0, 0, "Min: %f Max: %f", min_val, max_val);

    uint16_t frames_per_column = (audio_buffer->capacity / 2) / debug_window_width;
    uint16_t count = 0; 
    bool silence = false;

    for (uint16_t i = 0; i < debug_window_width; i++)
    {
        silence = false;
        float frame_sums_stereo[2] = { 0.0f, 0.0f };

        for (uint16_t j = 0; j < frames_per_column; j++)
        {
            count = j + (i * frames_per_column); 

            int mid_color = count == 0 ? COLOR_PAIR_BG_CYAN : COLOR_PAIR_BG_BLUE;

            wattron( debug_window,  COLOR_PAIR(mid_color));
            mvwaddch(debug_window, audiovis_mid_row,     i, ' ');
            mvwaddch(debug_window, audiovis_mid_row * 3, i, ' ');
            wattroff(debug_window, COLOR_PAIR(mid_color));

            float val = 0.0f;

            if (!ring_peek(audio_buffer, count, &val, false))
            {
                silence = true;
            }

            frame_sums_stereo[count % 2 != 0] += val;
        }

        for (int8_t j = 0; j < settings->audio_channels; j++)
        {
            float value = norm * (frame_sums_stereo[j] / (frames_per_column * 0.5f));
            float dir = value > 0.0f ? 1.0f : -1.0f;
            
            int max_height_abs = audiovis_mid_row * value * dir;
            int color_pair = silence ? (value > 0.0f ? COLOR_PAIR_BG_CYAN : COLOR_PAIR_BG_BLUE)
                : (value > 0.0f ? COLOR_PAIR_BG_GREEN : COLOR_PAIR_BG_RED);

            wattron(debug_window, COLOR_PAIR(color_pair));

            for (int k = 1; k <= max_height_abs; k++)
            {
                mvwaddch(debug_window, (audiovis_mid_row * (1 + (2*j)))+(k*dir*-1), i, ' ');
            }

            wattroff(debug_window, COLOR_PAIR(color_pair));
        }
    }

    for (uint16_t i = 0; i < (uint16_t)(debug_window_height * (volume * 0.5f)); i++)
    {
        wattron( debug_window, COLOR_PAIR(COLOR_PAIR_BG_MAGENTA));
        mvwaddch(debug_window, debug_window_height - i, 0, ' ');
        wattroff(debug_window, COLOR_PAIR(COLOR_PAIR_BG_MAGENTA));
    }

    wrefresh(debug_window);
}

void input_init(PlatformSettings_t *settings, UniformRing_t **input_buffer_pptr)
{
    keypad(main_window, true);
    /// init input buffer
    *input_buffer_pptr = ring_create(settings->input_buffer_capacity, sizeof(int));
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

    gfx_debug_mode = GFX_DEBUG_NONE;

    main_window_full_width =     settings->gfx_buffer_width;
    main_window_full_height =    settings->gfx_buffer_height;

    main_window_current_width =  main_window_full_width;
    main_window_current_height = main_window_full_height;

    main_window_partial_width =   main_window_full_width * 0.5f;
    main_window_partial_height = main_window_full_height;

    debug_window_width =  main_window_full_width * 0.5f;
    debug_window_height = main_window_full_height;
    audiovis_mid_row =    debug_window_height / 4;

    main_window  = newwin(main_window_full_height,  main_window_full_width,  0, 0);
    debug_window = newwin(debug_window_height, debug_window_width, 0, main_window_partial_width);

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
