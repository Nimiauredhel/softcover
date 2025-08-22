#include "terminal_utils.h"
#include "terminal_ncurses.h"

#include <stdlib.h>
#include <signal.h>
#include <string.h>

#define LOADBMP_IMPLEMENTATION
#include "loadbmp.h"

#include "tinywav.h"

/**
 * Global flag set by OS termination signals
 * and polled by functions to allow graceful termination.
 */
bool should_terminate = false;

/**
 * The random_range() function uses this to determine
 * whether rand() was already seeded or not.
 */
static bool random_was_seeded = false;

/**
 * @brief Hooks up OS signals to a custom handler.
 */
void initialize_signal_handler(void)
{
    should_terminate = false;

    struct sigaction action;
    explicit_bzero(&action, sizeof(action));
    action.sa_handler = signal_handler;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
}

// calling random_range once to ensure that random is seeded
void initialize_random_seed(void)
{
    usleep(random_range(1234, 5678));
}

/**
 * @brief Handles selected OS signals.
 * Termination signals are caught to set the should_terminate flag
 * which signals running functions that they should attempt graceful termination.
 */
void signal_handler(int signum)
{
    switch (signum)
    {
        case SIGINT:
        case SIGTERM:
        case SIGHUP:
            should_terminate = true;
            break;
    }
}

/**
 * @brief Returns a random int between min and max.
 * Also checks if rand() was already seeded.
 */
int random_range(int min, int max)
{
    if (!random_was_seeded)
    {
        srand(time(NULL) + getpid());
        random_was_seeded = true;
    }

    int random_number = rand();
    random_number = (random_number % (max - min + 1)) + min;
    return random_number;
}

/**
 * @brief Returns the seconds elapsed since a given clock value.
 */
float seconds_since_clock(struct timespec *start_clock)
{
    struct timespec now_clock;
    clock_gettime(CLOCK_MONOTONIC, &now_clock);
    float elapsed_float = (now_clock.tv_nsec - start_clock->tv_nsec) / 1000000000.0;
    elapsed_float += (now_clock.tv_sec - start_clock->tv_sec);
    return elapsed_float;
}

void gfx_load_texture(char *name, Texture_t *dest)
{
    static char debug_buff[128] = {0};

    uint8_t *temp_buff = NULL;

    uint32_t width;
    uint32_t height;

    uint8_t dst_pixel_size_bytes = 1; // ncurses 8 color

    uint32_t ret = loadbmp_decode_file(name, &temp_buff, &width, &height, LOADBMP_RGB);

    size_t dst_size = width * height * dst_pixel_size_bytes;

    if (ret != 0)
    {
        snprintf(debug_buff, sizeof(debug_buff), "Error code %d while loading %s (%dx%dx%d).",
            ret, name, dest->width, dest->height, dest->pixel_size_bytes);
        debug_log(debug_buff);
        debug_break();
    }

    if (temp_buff != NULL)
    {
        dest->width = width;
        dest->height = height;
        dest->pixel_size_bytes = dst_pixel_size_bytes;

        for (uint16_t pixel = 0; pixel < dst_size; pixel++)
        {
            uint16_t src_pixel = pixel * 3;
            dest->pixels[pixel] = gfx_rgb_to_color_pair(temp_buff[src_pixel], temp_buff[src_pixel+1], temp_buff[src_pixel+2]);
        }

        free(temp_buff);

        snprintf(debug_buff, sizeof(debug_buff), "Loaded %s (%dx%dx%d).",
            name, dest->width, dest->height, dest->pixel_size_bytes);
        debug_log(debug_buff);
    }
}

void audio_load_wav(char *name, AudioClip_t *dest)
{
#define BLOCK_SIZE (256)

    static char debug_buff[128] = {0};

    TinyWav tw;
    tinywav_open_read(&tw, name,
    TW_INTERLEAVED); // LRLRLRLRLRLRLRLRLR

    uint32_t num_samples = tw.h.Subchunk2Size / tw.h.NumChannels * tw.h.BitsPerSample / 8;
    uint32_t num_blocks = num_samples / BLOCK_SIZE;
    size_t clip_size = sizeof(AudioClip_t) + (sizeof(float) * num_samples);

    snprintf(debug_buff, sizeof(debug_buff), "Loading WAV file '%s', size: %lu bytes.", name, clip_size);
    debug_log(debug_buff);

    memset(dest, 0, clip_size);
    dest->num_samples = num_samples;

    for (uint32_t i = 0; i < num_blocks; i++) 
    {
        tinywav_read_f(&tw, dest->samples+(num_blocks*i), BLOCK_SIZE);
    }

    tinywav_close_read(&tw);
}
