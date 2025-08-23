#ifndef TERMINAL_NCURSES_H
#define TERMINAL_NCURSES_H

#include <ncurses.h>

#include "common_interface.h"
#include "common_structs.h"
#include "terminal_debug.h"

uint8_t gfx_rgb_to_color_pair(uint8_t r, uint8_t g, uint8_t b);
char input_read(void);
void input_push_to_buffer(PlatformSettings_t *settings, ByteRing_t *input_buffer);
void gfx_refresh_debug_window(DebugRing_t *debug_ring, bool is_break);
void gfx_clear_buffer(Texture_t *gfx_buffer);
void gfx_sync_buffer(Texture_t *gfx_buffer);
void gfx_audio_vis(const FloatRing_t *audio_buffer);
void input_init(PlatformSettings_t *settings, ByteRing_t **input_buffer_pptr);
bool gfx_is_initialized(void);
void gfx_init(PlatformSettings_t *settings, Texture_t **gfx_buffer);
void gfx_deinit(void);

#endif
