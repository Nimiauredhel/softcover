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
    Thing_t first_thing;
    Thing_t second_thing;
    Thing_t *controlled_thing;
    uint16_t audio_sample_length;
    Memory_t *first_audio_sample;
    Memory_t *second_audio_sample;
    float *sample_to_play;
} AppMemory_t;

/// must match prototype @ref AppInitFunc
void app_init(Platform_t *platform, Memory_t **memory_pptr)
{
    Memory_t *memory = platform->memory_allocate(sizeof(AppMemory_t));
    *memory_pptr = memory;

    AppMemory_t *app_memory = (AppMemory_t *)memory->buffer;

    app_memory->first_thing.texture_id = platform->gfx_load_texture("first thing");
    app_memory->first_thing.x = 0;
    app_memory->first_thing.y = 0;

    app_memory->second_thing.texture_id = platform->gfx_load_texture("second thing");
    app_memory->second_thing.x = 5;
    app_memory->second_thing.y = 5;

    app_memory->audio_sample_length = 1024;
    app_memory->first_audio_sample = platform->memory_allocate(sizeof(float) * app_memory->audio_sample_length);
    app_memory->second_audio_sample = platform->memory_allocate(sizeof(float) * app_memory->audio_sample_length);

    for (int i = 0; i < app_memory->audio_sample_length; i+=2)
    {
        ((float *)app_memory->first_audio_sample->buffer)[i] = -1.0f + (2 * (float)i/(float)app_memory->audio_sample_length);
        ((float *)app_memory->first_audio_sample->buffer)[i+1] = -1.0f + (2 * (float)i/(float)app_memory->audio_sample_length);

        ((float *)app_memory->second_audio_sample->buffer)[i] = sinf((float)i/(float)app_memory->audio_sample_length);
        ((float *)app_memory->second_audio_sample->buffer)[i+1] = sinf((float)i/(float)app_memory->audio_sample_length);
    }

    app_memory->controlled_thing = &app_memory->first_thing;
    app_memory->sample_to_play = (float *)app_memory->first_audio_sample->buffer;
}

/// must match prototype @ref AppLoopFunc
void app_loop(Platform_t *platform, Memory_t *memory)
{
    static const int8_t mov_speed = 1;

    AppMemory_t *app_memory = (AppMemory_t *)memory->buffer;

    char input = platform->input_read();

    switch (input)
    {
        case 'w':
            app_memory->controlled_thing->y -= mov_speed;
            platform->audio_play_chunk(app_memory->sample_to_play, app_memory->audio_sample_length);
            break;
        case 'a':
            app_memory->controlled_thing->x -= mov_speed;
            platform->audio_play_chunk(app_memory->sample_to_play, app_memory->audio_sample_length);
            break;
        case 's':
            app_memory->controlled_thing->y += mov_speed;
            platform->audio_play_chunk(app_memory->sample_to_play, app_memory->audio_sample_length);
            break;
        case 'd':
            app_memory->controlled_thing->x += mov_speed;
            platform->audio_play_chunk(app_memory->sample_to_play, app_memory->audio_sample_length);
            break;
        case 'q':
            if (app_memory->controlled_thing == &app_memory->first_thing)
            {
                app_memory->controlled_thing = &app_memory->second_thing;
                app_memory->sample_to_play = (float *)app_memory->second_audio_sample->buffer;
            }
            else
            {
                app_memory->controlled_thing = &app_memory->first_thing;
                app_memory->sample_to_play = (float *)app_memory->first_audio_sample->buffer;
            }
            break;
        default:
            break;
    }

    platform->gfx_clear_buffer();
    platform->gfx_draw_texture(app_memory->first_thing.texture_id, app_memory->first_thing.x, app_memory->first_thing.y);
    platform->gfx_draw_texture(app_memory->second_thing.texture_id, app_memory->second_thing.x, app_memory->second_thing.y);
}
