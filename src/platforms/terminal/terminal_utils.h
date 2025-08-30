#ifndef TERMINAL_UTILS_H
#define TERMINAL_UTILS_H

#include <stdbool.h>

#include "common_structs.h"

/**
 * @brief Global flag set by OS termination signals
 * and polled by functions to allow graceful termination.
 */
extern bool should_terminate;

void initialize_signal_handler(void);
void initialize_random_seed(void);
void signal_handler(int signum);
int random_range(int min, int max);
size_t storage_load_text(const char *name, char *dest, size_t max_len);
bool gfx_load_texture(char *name, Texture_t *dest, size_t max_size);
bool audio_load_wav(char *name, AudioClip_t *dest, size_t max_size);

#endif
