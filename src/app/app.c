#include <stdint.h>
#include <math.h>
#include <stdio.h>

#include "softcover_platform.h"

typedef struct Thing
{
    uint16_t texture_idx;
    int16_t x;
    int16_t y;
} Thing_t;

typedef struct AppMemory
{
    Thing_t things[2];
    uint16_t controlled_thing;
    float audio_samples[2][1024];
    uint16_t audio_sample_length;
    uint16_t scratch_size;
    uint16_t scratch_used;
    uint8_t scratch_buff[32768];
} AppMemory_t;

static uint16_t load_texture_to_scratch(char *name, const Platform_t *platform, AppMemory_t *app_memory)
{
    uint16_t index = app_memory->scratch_used;
    TextureRGB_t *texture_ptr = (TextureRGB_t *)(app_memory->scratch_buff+index);
    platform->gfx_load_texture(name, texture_ptr);
    uint16_t size = sizeof(TextureRGB_t) + (texture_ptr->height * texture_ptr->width * 1);
    app_memory->scratch_used += size;
    return index;
}

/// must match prototype @ref AppInitFunc
void app_init(const Platform_t *platform, Memory_t **memory_pptr)
{
    Memory_t *memory = platform->memory_allocate(sizeof(AppMemory_t));
    *memory_pptr = memory;

    AppMemory_t *app_memory = (AppMemory_t *)memory->buffer;

    app_memory->scratch_size = sizeof(app_memory->scratch_buff);
    app_memory->scratch_used = 0;

    app_memory->things[0].texture_idx = load_texture_to_scratch("thing1.bmp", platform, app_memory);
    app_memory->things[0].x = 0;
    app_memory->things[0].y = 1;

    app_memory->things[1].texture_idx = load_texture_to_scratch("thing2.bmp", platform, app_memory);
    app_memory->things[1].x = 5;
    app_memory->things[1].y = 4;

    app_memory->audio_sample_length = 1024;

    for (int i = 0; i < app_memory->audio_sample_length; i+=2)
    {
        app_memory->audio_samples[0][i] = -1.0f + (2 * (float)i/(float)app_memory->audio_sample_length);
        app_memory->audio_samples[0][i+1] = -1.0f + (2 * (float)i/(float)app_memory->audio_sample_length);

        app_memory->audio_samples[1][i] = sinf((float)i/(float)app_memory->audio_sample_length);
        app_memory->audio_samples[1][i+1] = sinf((float)i/(float)app_memory->audio_sample_length);
    }

    app_memory->controlled_thing = 0;
}

/// must match prototype @ref AppLoopFunc
void app_loop(const Platform_t *platform, Memory_t *memory)
{
    static const int8_t mov_speed = 1;
    
    static char debug_buff[128] = {0};

    AppMemory_t *app_memory = (AppMemory_t *)memory->buffer;

    char input = platform->input_read();

    switch (input)
    {
        case 'w':
            app_memory->things[app_memory->controlled_thing].y -= mov_speed;
            platform->audio_play_chunk(app_memory->audio_samples[app_memory->controlled_thing], app_memory->audio_sample_length);
            snprintf(debug_buff, sizeof(debug_buff), "Thing %d moved to [%d,%d]", app_memory->controlled_thing,
                    app_memory->things[app_memory->controlled_thing].x, app_memory->things[app_memory->controlled_thing].y);
            platform->debug_log(debug_buff);
            break;
        case 'a':
            app_memory->things[app_memory->controlled_thing].x -= mov_speed;
            platform->audio_play_chunk(app_memory->audio_samples[app_memory->controlled_thing], app_memory->audio_sample_length);
            snprintf(debug_buff, sizeof(debug_buff), "Thing %d moved to [%d,%d]", app_memory->controlled_thing,
                    app_memory->things[app_memory->controlled_thing].x, app_memory->things[app_memory->controlled_thing].y);
            platform->debug_log(debug_buff);
            break;
        case 's':
            app_memory->things[app_memory->controlled_thing].y += mov_speed;
            platform->audio_play_chunk(app_memory->audio_samples[app_memory->controlled_thing], app_memory->audio_sample_length);
            snprintf(debug_buff, sizeof(debug_buff), "Thing %d moved to [%d,%d]", app_memory->controlled_thing,
                    app_memory->things[app_memory->controlled_thing].x, app_memory->things[app_memory->controlled_thing].y);
            platform->debug_log(debug_buff);
            break;
        case 'd':
            app_memory->things[app_memory->controlled_thing].x += mov_speed;
            platform->audio_play_chunk(app_memory->audio_samples[app_memory->controlled_thing], app_memory->audio_sample_length);
            snprintf(debug_buff, sizeof(debug_buff), "Thing %d moved to [%d,%d]", app_memory->controlled_thing,
                    app_memory->things[app_memory->controlled_thing].x, app_memory->things[app_memory->controlled_thing].y);
            platform->debug_log(debug_buff);
            break;
        case 'q':
            app_memory->controlled_thing = app_memory->controlled_thing == 0 ? 1 : 0;
            break;
        case '1':
            platform->storage_load_state("test_save");
            break;
        case '!':
            platform->storage_save_state("test_save");
            break;
        default:
            break;
    }

    platform->gfx_clear_buffer();
    platform->gfx_draw_texture((TextureRGB_t *)(app_memory->scratch_buff+app_memory->things[0].texture_idx), app_memory->things[0].x, app_memory->things[0].y);
    platform->gfx_draw_texture((TextureRGB_t *)(app_memory->scratch_buff+app_memory->things[1].texture_idx), app_memory->things[1].x, app_memory->things[1].y);
}

/// must match prototype @ref AppExitFunc
void app_exit(const Platform_t *platform, Memory_t *memory)
{
}
