#include "softcover_sdl2.h"
#include "softcover_utils.h"
#include "softcover_debug.h"
#include "softcover_time.h"

#include <string.h>

static bool sdl_gfx_is_initialized = false;
static GfxDebugMode_t gfx_debug_mode = 0;

static SDL_Window *main_window = NULL;
static SDL_Window *debug_window = NULL;

static SDL_Surface *main_surface = NULL;
static SDL_Surface *main_pixels = NULL;

static uint16_t main_window_width;
static uint16_t main_window_height;

static uint16_t debug_window_width;
static uint16_t debug_window_height;

static int audiovis_mid_row;

GfxDebugMode_t gfx_get_debug_mode(void)
{
    return gfx_debug_mode;
}

void gfx_toggle_debug_mode(void)
{
    gfx_debug_mode = (gfx_debug_mode + 1) % GFX_DEBUG_MAXVAL;
    /*

    switch(gfx_debug_mode)
    {
    case GFX_DEBUG_NONE:
        main_window_current_width =  main_window_width;
        main_window_current_height = main_window_height;
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
    */
}

void gfx_refresh_debug_window(DebugRing_t *debug_ring, bool is_break)
{
    /*
    if (gfx_debug_mode != GFX_DEBUG_LOG) return;
    werase(debug_window);

    wattron(debug_window, COLOR_PAIR(COLOR_PAIR_BG_RED));
    mvwprintw(debug_window, 0, 0, "%s", is_break ? "BREAK" : "DEBUG");
    wattroff(debug_window, COLOR_PAIR(COLOR_PAIR_BG_RED));

    int32_t mod = debug_ring->len - (debug_window_height-1);
    if (mod < 0) mod = 0;
    uint16_t idx = (debug_ring->head + mod) % DEBUG_RING_CAPACITY;;

    for (uint16_t i = 1; i < debug_window_height; i++)
    {
        wattron(debug_window, COLOR_PAIR(COLOR_PAIR_BG_BLACK));
        mvwprintw(debug_window, i, 0, "[%u]%s", idx, debug_ring->debug_messages[idx]);
        wattroff(debug_window, COLOR_PAIR(COLOR_PAIR_BG_RED));

        idx = (idx + 1) % DEBUG_RING_CAPACITY;
    }

    wrefresh(debug_window);
    */
}

bool input_try_read(InputEvent_t *out)
{
    static SDL_Event event = {0};
    static SDL_WindowEvent *w_event = (SDL_WindowEvent *)&event;

    bool success = false;

    if ((success = SDL_PollEvent(&event)))
    {
        switch (event.type)
        {
            case SDL_KEYUP:
                out->value = 0;
                out->key = event.key.keysym.sym;
                break;
            case SDL_WINDOWEVENT:
                switch (w_event->event)
                {
                    case SDL_WINDOWEVENT_CLOSE:
                        should_terminate = true;
                        break;
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        /// TODO: handle window size change and possibly other window events
                        break;
                }
                break;
            case SDL_QUIT:
            case SDL_APP_TERMINATING:
                should_terminate = true;
                break;
            case SDL_KEYDOWN:
                out->value = 1;
                out->key = event.key.keysym.sym;
                break;
            default:
                break;
        }
    }

    return success;
}

void input_push_to_buffer(PlatformSettings_t *settings, UniformRing_t *input_buffer)
{
    bool buffer_blocked = false;
    InputEvent_t e = {0};

    while ((!buffer_blocked) && input_try_read(&e))
    {
        if (e.key == SDLK_LEFT && e.value == 1)
        {
            gfx_toggle_debug_mode();
            continue;
        }

        ring_push(input_buffer, &e, 1, false);
    }
}

void gfx_sync_buffer(Texture_t *gfx_buffer)
{
    SDL_LockSurface(main_pixels);
    SDL_FillRect(main_pixels, NULL, 0);

    memcpy(main_pixels->pixels, gfx_buffer->pixels, gfx_buffer->height*gfx_buffer->width*gfx_buffer->pixel_size_bytes);
    /*
    for (size_t i = 0; i < gfx_buffer->width * gfx_buffer->height * gfx_buffer->pixel_size_bytes; i++)
    {
        ((uint8_t *)main_pixels->pixels)[i] = gfx_buffer->pixels[i];
    }
    */
    /*
    for (int32_t y = 0; y < gfx_buffer->height && y < main_pixels->h; y++)
    {
        int32_t row = y * gfx_buffer->width;

        for (int32_t x = 0; x < gfx_buffer->height && x < main_pixels->w; x++)
        {
            int32_t src_idx = 3 * (row + x);
            int32_t dst_idx = 4 * (row + x);

            ((uint8_t *)main_pixels->pixels)[dst_idx+3] = 255;

            for (uint8_t sp = 0; sp < 3; sp++)
            {
                ((uint8_t *)main_pixels->pixels)[dst_idx+sp] = gfx_buffer->pixels[src_idx+sp];
            }
        }
    }
    */
    SDL_UnlockSurface(main_pixels);
    SDL_BlitScaled(main_pixels, NULL, main_surface, NULL);
    SDL_UpdateWindowSurface(main_window);
}

void gfx_audio_vis(const UniformRing_t *audio_buffer, const PlatformSettings_t *settings, float volume)
{
    /*
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
    */
}

void input_init(PlatformSettings_t *settings, UniformRing_t **input_buffer_pptr)
{
    SDL_Init(SDL_INIT_EVENTS);
    *input_buffer_pptr = ring_create(settings->input_buffer_capacity, sizeof(InputEvent_t));
}

bool gfx_is_initialized(void)
{
    return sdl_gfx_is_initialized;
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

    /// init sdl2

    TTF_Init();
    SDL_VideoInit(NULL);

    gfx_debug_mode = GFX_DEBUG_NONE;

    float ratio = 1080.0f / settings->gfx_buffer_width;
    main_window_height =    settings->gfx_buffer_height * ratio;
    main_window_width =     1080;

    debug_window_width =  main_window_width * 0.5f;
    debug_window_height = main_window_height * 0.5f;

    audiovis_mid_row =    debug_window_height / 4;

    main_window = SDL_CreateWindow("Softcover Main", 0, 0, main_window_width, main_window_height, SDL_WINDOW_SHOWN);
    debug_window = SDL_CreateWindow("Softcover Debug", 0, 0, debug_window_width, debug_window_height, SDL_WINDOW_HIDDEN);

    main_surface = SDL_GetWindowSurface(main_window);
    main_pixels = SDL_CreateRGBSurfaceWithFormat(0, settings->gfx_buffer_width, settings->gfx_buffer_height, 24, SDL_PIXELFORMAT_RGB24);

    SDL_LockSurface(main_pixels);
    SDL_FillRect(main_pixels, NULL, 0);
    SDL_UnlockSurface(main_pixels);
    SDL_BlitSurface(main_pixels, NULL, main_surface, NULL);
    SDL_UpdateWindowSurface(main_window);

    SDL_RaiseWindow(main_window);

    sdl_gfx_is_initialized = true;
    debug_log("Gfx initialized.");
}

void gfx_deinit(void)
{

    if (main_window != NULL) SDL_DestroyWindow(main_window);

    if (debug_window != NULL) SDL_DestroyWindow(debug_window);

    sdl_gfx_is_initialized = false;
}
