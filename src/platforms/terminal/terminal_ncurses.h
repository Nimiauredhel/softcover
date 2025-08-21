#ifndef TERMINAL_NCURSES_H
#define TERMINAL_NCURSES_H

#include <ncurses.h>

#include "common_interface.h"
#include "common_structs.h"

uint8_t gfx_rgb_to_color_pair(uint8_t r, uint8_t g, uint8_t b);
char input_read(void);
void gfx_clear_buffer(Texture_t *gfx_buffer);
void gfx_sync_buffer(Texture_t *gfx_buffer);
void debug_log(char *message);
void debug_break(void);
void gfx_audio_vis(const FloatRing_t *audio_buffer);
void debug_init(void);
void gfx_init(PlatformSettings_t *settings, Texture_t **gfx_buffer);
void gfx_deinit(void);

#endif
