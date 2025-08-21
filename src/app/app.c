#include <stdint.h>
#include <math.h>
#include <stdio.h>

#include "softcover_platform.h"

#define APP_TEST_AUDIO_SAMPLE_LEN (8192)

typedef struct Thing
{
    size_t texture_idx;
    int16_t x;
    int16_t y;
} Thing_t;

typedef struct AppMemory
{
    Thing_t things[2];
    uint16_t controlled_thing;
    float audio_samples[2][APP_TEST_AUDIO_SAMPLE_LEN];
    size_t audio_sample_length;
    size_t scratch_size;
    size_t scratch_used;
    uint8_t scratch_buff[32768];
} AppMemory_t;

static size_t load_texture_to_scratch(char *name, const Platform_t *platform, AppMemory_t *app_memory)
{
    size_t index = app_memory->scratch_used;
    Texture_t *texture_ptr = (Texture_t *)(app_memory->scratch_buff+index);
    platform->gfx_load_texture(name, texture_ptr);
    size_t size = sizeof(Texture_t) + (texture_ptr->height * texture_ptr->width * 3);
    app_memory->scratch_used += size;
    return index;
}

/// must match prototype @ref AppSetupFunc
void app_setup(Platform_t *platform)
{
    static char debug_buff[128] = {0};

    size_t required_memory = sizeof(AppMemory_t);
    bool sufficient = platform->capabilities->app_memory_limit_bytes >= required_memory;

    snprintf(debug_buff, sizeof(debug_buff), "App requires %lu bytes of memory, Platform offers up to %lu bytes - %s.",
            required_memory, platform->capabilities->app_memory_limit_bytes, sufficient ? "proceeding" : "aborting");
    platform->debug_log(debug_buff);

    if (sufficient)
    {
        platform->settings->app_memory_required_bytes = required_memory;
    }
    else
    {
        platform->set_should_terminate(true);
    }
}

/// must match prototype @ref AppInitFunc
void app_init(const Platform_t *platform, Memory_t *memory)
{
    AppMemory_t *app_memory = (AppMemory_t *)memory->buffer;

    app_memory->scratch_size = sizeof(app_memory->scratch_buff);
    app_memory->scratch_used = 0;

    app_memory->things[0].texture_idx = load_texture_to_scratch("thing1.bmp", platform, app_memory);
    app_memory->things[0].x = 0;
    app_memory->things[0].y = 1;

    app_memory->things[1].texture_idx = load_texture_to_scratch("thing2.bmp", platform, app_memory);
    app_memory->things[1].x = 5;
    app_memory->things[1].y = 4;

    app_memory->audio_sample_length = APP_TEST_AUDIO_SAMPLE_LEN;

    for (size_t i = 0; i < app_memory->audio_sample_length; i+=2)
    {
        float sample_percent_f = (float)i/(float)app_memory->audio_sample_length;

        float values[5] =
        {
            (1.0f - sample_percent_f) * cosf(sample_percent_f),
            (1.0f - sample_percent_f) * cosf(32.0f * sample_percent_f),
            (1.0f - sample_percent_f) * -0.9f,
            (1.0f - sample_percent_f) * 0.9f,
            0.0f,
        };

        app_memory->audio_samples[0][i] = (-1.0f + sample_percent_f) * values[1 + (3 * (sample_percent_f > 0.75f)) - (sample_percent_f > 0.9f)];
        app_memory->audio_samples[0][i+1] = values[1 + (3 * (sample_percent_f > 0.75f)) - (sample_percent_f > 0.9f)];

        app_memory->audio_samples[1][i] = values[3 - (2 * (sample_percent_f < 0.75f))];
        app_memory->audio_samples[1][i+1] = values[3 - (2 * (sample_percent_f < 0.75f))];
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
    platform->gfx_draw_texture((Texture_t *)(app_memory->scratch_buff+app_memory->things[0].texture_idx), app_memory->things[0].x, app_memory->things[0].y);
    platform->gfx_draw_texture((Texture_t *)(app_memory->scratch_buff+app_memory->things[1].texture_idx), app_memory->things[1].x, app_memory->things[1].y);
}

/// must match prototype @ref AppExitFunc
void app_exit(const Platform_t *platform, Memory_t *memory)
{
}
