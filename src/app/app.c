#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "common_interface.h"
#include "common_structs.h"

#define APP_TEST_AUDIO_SAMPLE_LEN (8192)
#define APP_SCRATCH_SIZE (32768)
#define APP_THINGS_COUNT (8)

typedef struct Thing
{
    size_t texture_idx;
    int16_t x_pos;
    int16_t y_pos;
    int16_t x_offset;
    int16_t y_offset;
} Thing_t;

typedef struct AppScratch
{
    size_t used;
    uint8_t buff[APP_SCRATCH_SIZE];
} AppScratch_t;

typedef struct AppSerializables
{
    bool initialized;
    uint16_t things_count;
    Thing_t things[APP_THINGS_COUNT];
    uint16_t controlled_thing_idx;
    uint16_t mov_speed;
} AppSerializable_t;

typedef struct AppEphemera
{
    float audio_samples[2][APP_TEST_AUDIO_SAMPLE_LEN];
    size_t audio_sample_length;
    uint16_t things_draw_order[APP_THINGS_COUNT];
    AppScratch_t scratch;
} AppEphemeral_t;

static AppMemoryPartition_t *app = NULL;

static char input_read_from_buffer(ByteRing_t *input_buffer);
static void input_process_all(const Platform_t *platform, AppSerializable_t *serializables, AppEphemeral_t *ephemerals);
static size_t load_texture_to_scratch(char *name, const Platform_t *platform, AppScratch_t *scratch);
static void audio_push_sample(float *sample, uint16_t len);
static void gfx_clear_buffer(void);
static void gfx_draw_texture(Texture_t *texture, int start_x, int start_y);

static char input_read_from_buffer(ByteRing_t *input_buffer)
{
    if (app == NULL || app->serializable == NULL) return '~';

    char c = '~';

    if (byte_ring_pop(input_buffer, (uint8_t *)&c)) return c;
    return '~';
}

static void input_process_all(const Platform_t *platform, AppSerializable_t *serializables, AppEphemeral_t *ephemerals)
{
    static char debug_buff[128] = {0};

    if (app == NULL || app->serializable == NULL) return;

    char input = '~';

    do
    {
        input = input_read_from_buffer(app->input_buffer);

        switch (input)
        {
            case 'w':
                serializables->things[serializables->controlled_thing_idx].y_pos -= serializables->mov_speed;
                audio_push_sample(ephemerals->audio_samples[serializables->controlled_thing_idx], ephemerals->audio_sample_length);
                snprintf(debug_buff, sizeof(debug_buff), "Thing %d moved to [%d,%d]", serializables->controlled_thing_idx,
                        serializables->things[serializables->controlled_thing_idx].x_pos, serializables->things[serializables->controlled_thing_idx].y_pos);
                platform->debug_log(debug_buff);
                break;
            case 'a':
                serializables->things[serializables->controlled_thing_idx].x_pos -= serializables->mov_speed;
                audio_push_sample(ephemerals->audio_samples[serializables->controlled_thing_idx], ephemerals->audio_sample_length);
                snprintf(debug_buff, sizeof(debug_buff), "Thing %d moved to [%d,%d]", serializables->controlled_thing_idx,
                        serializables->things[serializables->controlled_thing_idx].x_pos, serializables->things[serializables->controlled_thing_idx].y_pos);
                platform->debug_log(debug_buff);
                break;
            case 's':
                serializables->things[serializables->controlled_thing_idx].y_pos += serializables->mov_speed;
                audio_push_sample(ephemerals->audio_samples[serializables->controlled_thing_idx], ephemerals->audio_sample_length);
                snprintf(debug_buff, sizeof(debug_buff), "Thing %d moved to [%d,%d]", serializables->controlled_thing_idx,
                        serializables->things[serializables->controlled_thing_idx].x_pos, serializables->things[serializables->controlled_thing_idx].y_pos);
                platform->debug_log(debug_buff);
                break;
            case 'd':
                serializables->things[serializables->controlled_thing_idx].x_pos += serializables->mov_speed;
                audio_push_sample(ephemerals->audio_samples[serializables->controlled_thing_idx], ephemerals->audio_sample_length);
                snprintf(debug_buff, sizeof(debug_buff), "Thing %d moved to [%d,%d]", serializables->controlled_thing_idx,
                        serializables->things[serializables->controlled_thing_idx].x_pos, serializables->things[serializables->controlled_thing_idx].y_pos);
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
    }
    while(input != '~');

}

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

            /// extremely dumb alpha test
            /// TODO: somehow move this responsibility to the platform side
            if ((texture->pixel_size_bytes == 1 && texture->pixels[texture_idx] > 64)
             || (texture->pixel_size_bytes == 4 && texture->pixels[texture_idx+3] == 0))
            {
                continue;
            }

            for (uint8_t i = 0; i < app->gfx_buffer->pixel_size_bytes; i++)
            {
                app->gfx_buffer->pixels[buffer_idx+i] = texture->pixels[texture_idx+i];
            }
        }
    }
}

static void gfx_sort_things(AppSerializable_t *serializables, AppEphemeral_t *ephemerals)
{
    uint16_t temp;
    bool swapped = false;

    for (uint16_t end_idx = serializables->things_count-1; end_idx > 0; end_idx--)
    {
        swapped = false;

        for (uint16_t i = 0; i < end_idx; i++)
        {
            if (serializables->things[ephemerals->things_draw_order[i]].y_pos > serializables->things[ephemerals->things_draw_order[i+1]].y_pos)
            {
                swapped = true;
                temp = ephemerals->things_draw_order[i];
                ephemerals->things_draw_order[i] = ephemerals->things_draw_order[i+1];
                ephemerals->things_draw_order[i+1] = temp;
            }
        }

        if (!swapped) break;
    }
}

static void gfx_draw_all(AppSerializable_t *serializables, AppEphemeral_t *ephemerals)
{
    gfx_sort_things(serializables, ephemerals);

    for (uint16_t i = 0; i < serializables->things_count; i++)
    {
        uint16_t thing_idx = ephemerals->things_draw_order[i];

        gfx_draw_texture((Texture_t *)(ephemerals->scratch.buff+serializables->things[thing_idx].texture_idx),
            serializables->things[thing_idx].x_pos - serializables->things[thing_idx].x_offset,
            serializables->things[thing_idx].y_pos - serializables->things[thing_idx].y_offset);
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

    platform->settings->audio_buffer_capacity = 16384;

    platform->settings->input_buffer_capacity = 128;

    size_t required_state_memory = sizeof(AppSerializable_t) + sizeof(AppEphemeral_t);

    size_t required_memory_total = required_state_memory + required_gfx_memory
        + sizeof(FloatRing_t) + platform->settings->audio_buffer_capacity
        + sizeof(ByteRing_t) + platform->settings->input_buffer_capacity;
    
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
 * @brief Called on app start,
 * TODO: also call after serializable state is loaded, or following an executable hot-reload.
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
    }
    else
    {
        platform->debug_log("Initializing app serializable state.");

        serializables->things_count = APP_THINGS_COUNT;

        serializables->things[0].texture_idx = load_texture_to_scratch("thing1.bmp", platform, &ephemerals->scratch);
        serializables->things[0].x_pos = 0;
        serializables->things[0].y_pos = 1;
        serializables->things[0].x_offset = 3;
        serializables->things[0].y_offset = 7;

        serializables->things[1].texture_idx = load_texture_to_scratch("thing2.bmp", platform, &ephemerals->scratch);
        serializables->things[1].x_pos = 5;
        serializables->things[1].y_pos = 4;
        serializables->things[1].x_offset = 3;
        serializables->things[1].y_offset = 7;

        for (uint16_t i = 2; i < serializables->things_count; i++)
        {
            serializables->things[i].texture_idx = load_texture_to_scratch("apple.bmp", platform, &ephemerals->scratch);
            serializables->things[i].x_pos = 2+(i*4);
            serializables->things[i].y_pos = 4+(i*2);
            serializables->things[i].x_offset = 1;
            serializables->things[i].y_offset = 3;
        }

        serializables->controlled_thing_idx = 0;
        serializables->mov_speed = 1;

        serializables->initialized = true;
    }

    /// section of ephemerals that depends on serializables, proabably a bad idea for ~200 other reasons as well
    /// TODO: do this right
    for (uint16_t i = 0; i < serializables->things_count; i++)
    {
        ephemerals->things_draw_order[i] = i;
    }
}

/// must match prototype @ref AppLoopFunc
void app_loop(const Platform_t *platform)
{
    AppSerializable_t *serializables = (AppSerializable_t *)app->serializable->buffer;
    AppEphemeral_t *ephemerals = (AppEphemeral_t *)app->ephemeral->buffer;

    input_process_all(platform, serializables, ephemerals);

    gfx_clear_buffer();
    gfx_draw_all(serializables, ephemerals);
}

/// must match prototype @ref AppExitFunc
void app_exit(const Platform_t *platform)
{
}
