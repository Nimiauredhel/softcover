#ifndef TERMINAL_NCURSES_H
#define TERMINAL_NCURSES_H

#include <ncurses.h>

#include "softcover_platform.h"
#include "terminal_portaudio.h"

char input_read(void);
void gfx_clear_buffer(void);
void gfx_draw_texture(Texture_t *texture, int x, int y);
void gfx_sync_buffer(void);
void debug_log(char *message);
void debug_break(void);
void gfx_audio_vis(const AudioBuffer_t *audio_buffer);
void debug_init(void);
void gfx_init(void);
void gfx_deinit(void);

#endif
