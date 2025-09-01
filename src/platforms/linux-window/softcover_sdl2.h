#ifndef SOFTCOVER_SDL2_H
#define SOFTCOVER_SDL2_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_ttf.h>

#include "common_interface.h"
#include "common_structs.h"
#include "softcover_debug.h"

typedef enum GfxDebugMode
{
    GFX_DEBUG_NONE = 0,
    GFX_DEBUG_LOG = 1,
    GFX_DEBUG_AUDIO = 2,
    GFX_DEBUG_MAXVAL = 3,
} GfxDebugMode_t;

int input_read(void);
void input_push_to_buffer(PlatformSettings_t *settings, UniformRing_t *input_buffer);
GfxDebugMode_t gfx_get_debug_mode(void);
void gfx_toggle_debug_mode(void);
void gfx_refresh_debug_window(DebugRing_t *debug_ring, bool is_break);
void gfx_clear_buffer(Texture_t *gfx_buffer);
void gfx_sync_buffer(Texture_t *gfx_buffer);
void gfx_audio_vis(const UniformRing_t *audio_buffer, const PlatformSettings_t *settings, float volume);
void input_init(PlatformSettings_t *settings, UniformRing_t **input_buffer_pptr);
bool gfx_is_initialized(void);
void gfx_init(PlatformSettings_t *settings, Texture_t **gfx_buffer);
void gfx_deinit(void);

#endif
