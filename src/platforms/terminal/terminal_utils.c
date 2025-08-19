#include "terminal_utils.h"
#include "terminal_ncurses.h"

#include <stdlib.h>
#include <signal.h>
#include <string.h>

#define LOADBMP_IMPLEMENTATION
#include "loadbmp.h"

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

void gfx_load_texture(char *name, TextureRGB_t *dest)
{
    static char debug_buff[128] = {0};

    uint32_t ret = loadbmp_decode_file(name, &dest->pixels, &dest->width, &dest->height, LOADBMP_RGB);

    if (ret != 0)
    {
        snprintf(debug_buff, sizeof(debug_buff), "Error code %d while loading %s (%dx%d).",
            ret, name, dest->width, dest->height);
        debug_log(debug_buff);
        debug_break();
    }
}
