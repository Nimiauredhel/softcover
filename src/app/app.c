#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "common_interface.h"
#include "common_structs.h"

#define APP_TEST_AUDIO_SAMPLE_LEN (8192)
#define APP_SCRATCH_SIZE (32768)

typedef struct Thing
{
    size_t texture_idx;
    int16_t x;
    int16_t y;
} Thing_t;

typedef struct AppScratch
{
    size_t used;
    uint8_t buff[APP_SCRATCH_SIZE];
} AppScratch_t;

typedef struct AppSerializables
{
    bool initialized;
    Thing_t things[2];
    uint16_t controlled_thing_idx;
} AppSerializable_t;

typedef struct AppEphemera
{
    float audio_samples[2][APP_TEST_AUDIO_SAMPLE_LEN];
    size_t audio_sample_length;
    AppScratch_t scratch;
} AppEphemeral_t;

static AppMemoryPartition_t *app = NULL;

static size_t load_texture_to_scratch(char *name, const Platform_t *platform, AppScratch_t *scratch)
{
    static char debug_buff[128] = {0};

    snprintf(debug_buff, sizeof(debug_buff), "Loading texture '%s' to scratch memory.", name);
    platform->debug_log(debug_buff);

    size_t index = scratch->used;
    Texture_t *texture_ptr = (Texture_t *)(scratch->buff+index);
    platform->gfx_load_texture(name, texture_ptr);
    size_t size = sizeof(Texture_t) + (texture_ptr->height * texture_ptr->width * texture_ptr->pixel_size_bytes);
    scratch->used += size;
    return index;
}

static void audio_push_sample(float *sample, uint16_t len)
{
    if (app == NULL || app->audio_buffer == NULL) return;
    float_ring_push(app->audio_buffer, sample, len);
}

static void gfx_clear_buffer(void)
{
    if (app == NULL || app->gfx_buffer == NULL) return;
    memset(app->gfx_buffer->pixels, 0,
    app->gfx_buffer->width * app->gfx_buffer->height * app->gfx_buffer->pixel_size_bytes);
}

static void gfx_draw_texture(Texture_t *texture, int start_x, int start_y)
{
    if (app == NULL || app->gfx_buffer == NULL) return;

    int pixel_x;
    int pixel_y;

    for (uint16_t texture_y = 0; texture_y < texture->height; texture_y++)
    {
        pixel_y = start_y + texture_y;
        if (pixel_y >= app->gfx_buffer->height || pixel_y < 0) continue;

        for (uint16_t texture_x = 0; texture_x < texture->width; texture_x++)
        {
            pixel_x = start_x + texture_x;
            if (pixel_x >= app->gfx_buffer->width  || pixel_x < 0) continue;

            uint16_t texture_idx = texture->pixel_size_bytes * (texture_x + (texture_y * texture->width));
            uint16_t buffer_idx = app->gfx_buffer->pixel_size_bytes * (pixel_x + (pixel_y * app->gfx_buffer->width));

            for (uint8_t i = 0; i < app->gfx_buffer->pixel_size_bytes; i++)
            {
                app->gfx_buffer->pixels[buffer_idx+i] = texture->pixels[texture_idx+i];
            }
        }
    }
}

/// must match prototype @ref AppSetupFunc
void app_setup(Platform_t *platform)
{
    static char debug_buff[128] = {0};

    platform->settings->gfx_frame_time_target_us = 16000;

    platform->settings->gfx_pixel_size_bytes = 1;
    platform->settings->gfx_buffer_width = 160;
    platform->settings->gfx_buffer_height = 32;

    size_t required_gfx_memory = 
    platform->settings->gfx_pixel_size_bytes *
    platform->settings->gfx_buffer_width *
    platform->settings->gfx_buffer_height;

    platform->settings->audio_buffer_size = 16384;

    size_t required_state_memory = sizeof(AppSerializable_t) + sizeof(AppEphemeral_t);
    size_t required_memory_total = required_state_memory + required_gfx_memory + platform->settings->audio_buffer_size;
    
    bool sufficient = platform->capabilities->app_memory_max_bytes >= required_memory_total;

    snprintf(debug_buff, sizeof(debug_buff), "App requires %lu bytes of memory, Platform offers up to %lu bytes - %s.",
            required_memory_total, platform->capabilities->app_memory_max_bytes, sufficient ? "proceeding" : "aborting");
    platform->debug_log(debug_buff);

    if (sufficient)
    {
        platform->settings->app_memory_serializable_bytes = sizeof(AppSerializable_t);
        platform->settings->app_memory_ephemeral_bytes = sizeof(AppEphemeral_t);
    }
    else
    {
        platform->set_should_terminate(true);
    }
}

/**
 * @brief Called on app start, after serializable state is loaded, or following an executable hot-reload.
 * Must match prototype @ref AppInitFunc.
 */

void app_init(const Platform_t *platform, AppMemoryPartition_t *memory)
{
    platform->debug_log("App initializing fresh memory partition.");

    if (memory == NULL)
    {
        platform->debug_log("Null memory partition pointer provided by platform.");
        platform->set_should_terminate(true);
        return;
    }

    app = memory;

    platform->debug_log("Initializing app ephemeral state.");

    AppEphemeral_t *ephemerals = (AppEphemeral_t *)app->ephemeral->buffer;

    ephemerals->scratch.used = 0;

    ephemerals->audio_sample_length = APP_TEST_AUDIO_SAMPLE_LEN;

    for (size_t i = 0; i < ephemerals->audio_sample_length; i+=2)
    {
        float sample_percent_f = (float)i/(float)ephemerals->audio_sample_length;

        float values[5] =
        {
            (1.0f - sample_percent_f) * cosf(sample_percent_f),
            (1.0f - sample_percent_f) * cosf(32.0f * sample_percent_f),
            (1.0f - sample_percent_f) * -0.9f,
            (1.0f - sample_percent_f) * 0.9f,
            0.0f,
        };

        ephemerals->audio_samples[0][i] = (-1.0f + sample_percent_f) * values[1 + (3 * (sample_percent_f > 0.75f)) - (sample_percent_f > 0.9f)];
        ephemerals->audio_samples[0][i+1] = values[1 + (3 * (sample_percent_f > 0.75f)) - (sample_percent_f > 0.9f)];

        ephemerals->audio_samples[1][i] = values[3 - (2 * (sample_percent_f < 0.75f))];
        ephemerals->audio_samples[1][i+1] = values[3 - (2 * (sample_percent_f < 0.75f))];
    }

    AppSerializable_t *serializables = (AppSerializable_t *)app->serializable->buffer;

    if (serializables->initialized)
    {
        platform->debug_log("Loaded serializable state already initialized.");
        return;
    }
    else
    {
        platform->debug_log("Initializing app serializable state.");
    }

    serializables->things[0].texture_idx = load_texture_to_scratch("thing1.bmp", platform, &ephemerals->scratch);
    serializables->things[0].x = 0;
    serializables->things[0].y = 1;

    serializables->things[1].texture_idx = load_texture_to_scratch("thing2.bmp", platform, &ephemerals->scratch);
    serializables->things[1].x = 5;
    serializables->things[1].y = 4;

    serializables->controlled_thing_idx = 0;

    serializables->initialized = true;
}

/// must match prototype @ref AppLoopFunc
void app_loop(const Platform_t *platform)
{
    static const int8_t mov_speed = 1;
    
    static char debug_buff[128] = {0};

    AppSerializable_t *serializables = (AppSerializable_t *)app->serializable->buffer;
    AppEphemeral_t *ephemerals = (AppEphemeral_t *)app->ephemeral->buffer;

    char input = platform->input_read();

    switch (input)
    {
        case 'w':
            serializables->things[serializables->controlled_thing_idx].y -= mov_speed;
            audio_push_sample(ephemerals->audio_samples[serializables->controlled_thing_idx], ephemerals->audio_sample_length);
            snprintf(debug_buff, sizeof(debug_buff), "Thing %d moved to [%d,%d]", serializables->controlled_thing_idx,
                    serializables->things[serializables->controlled_thing_idx].x, serializables->things[serializables->controlled_thing_idx].y);
            platform->debug_log(debug_buff);
            break;
        case 'a':
            serializables->things[serializables->controlled_thing_idx].x -= mov_speed;
            audio_push_sample(ephemerals->audio_samples[serializables->controlled_thing_idx], ephemerals->audio_sample_length);
            snprintf(debug_buff, sizeof(debug_buff), "Thing %d moved to [%d,%d]", serializables->controlled_thing_idx,
                    serializables->things[serializables->controlled_thing_idx].x, serializables->things[serializables->controlled_thing_idx].y);
            platform->debug_log(debug_buff);
            break;
        case 's':
            serializables->things[serializables->controlled_thing_idx].y += mov_speed;
            audio_push_sample(ephemerals->audio_samples[serializables->controlled_thing_idx], ephemerals->audio_sample_length);
            snprintf(debug_buff, sizeof(debug_buff), "Thing %d moved to [%d,%d]", serializables->controlled_thing_idx,
                    serializables->things[serializables->controlled_thing_idx].x, serializables->things[serializables->controlled_thing_idx].y);
            platform->debug_log(debug_buff);
            break;
        case 'd':
            serializables->things[serializables->controlled_thing_idx].x += mov_speed;
            audio_push_sample(ephemerals->audio_samples[serializables->controlled_thing_idx], ephemerals->audio_sample_length);
            snprintf(debug_buff, sizeof(debug_buff), "Thing %d moved to [%d,%d]", serializables->controlled_thing_idx,
                    serializables->things[serializables->controlled_thing_idx].x, serializables->things[serializables->controlled_thing_idx].y);
            platform->debug_log(debug_buff);
            break;
        case 'q':
            serializables->controlled_thing_idx = serializables->controlled_thing_idx == 0 ? 1 : 0;
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

    gfx_clear_buffer();
    gfx_draw_texture((Texture_t *)(ephemerals->scratch.buff+serializables->things[0].texture_idx), serializables->things[0].x, serializables->things[0].y);
    gfx_draw_texture((Texture_t *)(ephemerals->scratch.buff+serializables->things[1].texture_idx), serializables->things[1].x, serializables->things[1].y);
}

/// must match prototype @ref AppExitFunc
void app_exit(const Platform_t *platform)
{
}
