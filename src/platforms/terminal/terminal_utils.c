#include "terminal_utils.h"
#include "terminal_ncurses.h"

#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>

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

size_t storage_load_text(const char *name, char *dest, size_t max_len)
{
    char debug_buff[DEBUG_MESSAGE_MAX_LEN] = {0};

    if (max_len <= 0) return 0;

    size_t remaining = max_len;
    size_t read = 0;

    FILE *file = fopen(name, "r");

    if (file == NULL)
    {
        int err = errno;
        snprintf(debug_buff, sizeof(debug_buff), "Failed to open [%s]: %s.", name, strerror(err));
        debug_log(debug_buff);
        return 0;
    }

    char *line = NULL;
    size_t line_len = 0;

    do
    {
        line = fgets(dest, remaining, file);

        if (line != NULL)
        {
            line_len = strlen(line);
            remaining -= line_len;
            read += line_len;
        }
        else
        {
            break;
        } 
    }
    while (remaining > 0);

    fclose(file);

    return read;
}

void gfx_load_texture(char *name, Texture_t *dest)
{
    static char debug_buff[DEBUG_MESSAGE_MAX_LEN] = {0};

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

    static char debug_buff[DEBUG_MESSAGE_MAX_LEN] = {0};

    TinyWav tw;
    tinywav_open_read(&tw, name,
    TW_INTERLEAVED); // LRLRLRLRLRLRLRLRLR

    uint8_t num_channels = tw.h.NumChannels;
    uint32_t num_samples = tw.h.Subchunk2Size / num_channels * tw.h.BitsPerSample / 8;
    size_t clip_size = sizeof(AudioClip_t) + ((sizeof(float) * num_samples));

    snprintf(debug_buff, sizeof(debug_buff), "Loading WAV file '%s', channels: %u,  size: %lu bytes.", name, num_channels, clip_size);
    debug_log(debug_buff);

    bzero(dest, clip_size);
    dest->num_channels = num_channels;
    dest->num_samples = num_samples;

    uint32_t index = 0;
    uint32_t remaining = num_samples;

    while(remaining > 0)
    {
        uint32_t to_write = remaining > BLOCK_SIZE ? BLOCK_SIZE : remaining;
        tinywav_read_f(&tw, &dest->samples[index], to_write);
        index += to_write;
        remaining -= to_write;
    }

    tinywav_close_read(&tw);
}
