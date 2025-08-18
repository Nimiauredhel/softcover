#include <stdint.h>
#include <math.h>

#include "softcover_platform.h"

typedef struct Thing
{
    uint8_t texture_id;
    int8_t x;
    int8_t y;
} Thing_t;

typedef struct AppMemory
{
    Thing_t things[2];
    uint16_t controlled_thing;
    float audio_samples[2][1024];
    uint16_t audio_sample_length;
} AppMemory_t;

/// must match prototype @ref AppInitFunc
void app_init(const Platform_t *platform, Memory_t **memory_pptr)
{
    Memory_t *memory = platform->memory_allocate(sizeof(AppMemory_t));
    *memory_pptr = memory;

    AppMemory_t *app_memory = (AppMemory_t *)memory->buffer;

    app_memory->things[0].texture_id = platform->gfx_load_texture("first thing");
    app_memory->things[0].x = 0;
    app_memory->things[0].y = 0;

    app_memory->things[1].texture_id = platform->gfx_load_texture("second thing");
    app_memory->things[1].x = 5;
    app_memory->things[1].y = 5;

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

    AppMemory_t *app_memory = (AppMemory_t *)memory->buffer;

    char input = platform->input_read();

    switch (input)
    {
        case 'w':
            app_memory->things[app_memory->controlled_thing].y -= mov_speed;
            platform->audio_play_chunk(app_memory->audio_samples[app_memory->controlled_thing], app_memory->audio_sample_length);
            break;
        case 'a':
            app_memory->things[app_memory->controlled_thing].x -= mov_speed;
            platform->audio_play_chunk(app_memory->audio_samples[app_memory->controlled_thing], app_memory->audio_sample_length);
            break;
        case 's':
            app_memory->things[app_memory->controlled_thing].y += mov_speed;
            platform->audio_play_chunk(app_memory->audio_samples[app_memory->controlled_thing], app_memory->audio_sample_length);
            break;
        case 'd':
            app_memory->things[app_memory->controlled_thing].x += mov_speed;
            platform->audio_play_chunk(app_memory->audio_samples[app_memory->controlled_thing], app_memory->audio_sample_length);
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
    platform->gfx_draw_texture(app_memory->things[0].texture_id, app_memory->things[0].x, app_memory->things[0].y);
    platform->gfx_draw_texture(app_memory->things[1].texture_id, app_memory->things[1].x, app_memory->things[1].y);
}

/// must match prototype @ref AppExitFunc
void app_exit(const Platform_t *platform, Memory_t *memory)
{
}
