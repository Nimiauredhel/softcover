#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "common_interface.h"
#include "common_structs.h"

#define APP_TEST_AUDIO_SAMPLE_LEN (8192)
#define APP_SCRATCH_SIZE (1024*2048)
#define APP_THINGS_COUNT (256)
#define APP_THINGS_LAYER_COUNT (6)
#define APP_CLIPS_COUNT (2)

typedef struct Thing
{
    bool used;
    size_t texture_idx;
    size_t move_sfx_idx;
    int16_t x_pos;
    int16_t y_pos;
    int16_t x_offset;
    int16_t y_offset;
    uint16_t coll_width;
    uint16_t coll_height;
    uint8_t layer;
} Thing_t;

typedef struct AppScratch
{
    size_t used;
    uint8_t buff[APP_SCRATCH_SIZE];
} AppScratch_t;

typedef struct AppSerializables
{
    bool initialized;
    uint16_t mov_speed;
    uint16_t things_count;
    uint16_t controlled_thing_idx;
    Thing_t things[APP_THINGS_COUNT];
    size_t audio_clip_indices[APP_CLIPS_COUNT];
} AppSerializable_t;

typedef struct AppEphemera
{
    char debug_buff[128];
    int32_t things_draw_order_layer_offsets[APP_THINGS_LAYER_COUNT];
    uint16_t things_draw_order[APP_THINGS_COUNT];
    AppScratch_t scratch;
} AppEphemeral_t;

static const Platform_t *platform = NULL;
static AppMemoryPartition_t *app = NULL;
static AppSerializable_t *serializables = NULL;
static AppEphemeral_t *ephemerals = NULL;

static char input_read_from_buffer(ByteRing_t *input_buffer);
static void input_process_all(void);
static size_t load_texture_to_scratch(char *name);
static size_t load_wav_to_scratch(char *name);
static void audio_push_samples(float *samples, uint32_t len);
static void audio_push_clip(AudioClip_t *clip);
static void things_initialize_draw_order(void);
static void things_update_draw_order(void);
static void things_get_distance(uint16_t first_thing_id, uint16_t second_thing_id, int32_t *x_dist_out, int32_t *y_dist_out);
static uint16_t thing_test_collision(uint16_t thing_id);
static void thing_move(uint16_t thing_idx, int16_t x_delta, int16_t y_delta);
static void things_sort_by_comparer(uint16_t start_idx, uint16_t end_idx, int8_t (*comparer)(Thing_t*, Thing_t*));
static void gfx_clear_buffer(void);
static void gfx_draw_texture(Texture_t *texture, int start_x, int start_y);
static void gfx_draw_thing(uint16_t thing_idx);
static void gfx_draw_all_things(void);

static char input_read_from_buffer(ByteRing_t *input_buffer)
{
    if (app == NULL || serializables == NULL) return '~';

    char c = '~';

    if (byte_ring_pop(input_buffer, (uint8_t *)&c)) return c;
    return '~';
}

static void input_process_all(void)
{
    if (platform == NULL || serializables == NULL) return;

    char input = '~';

    do
    {
        input = input_read_from_buffer(app->input_buffer);

        switch (input)
        {
            case 'w':
                thing_move(serializables->controlled_thing_idx, 0, -serializables->mov_speed);
                break;
            case 'a':
                thing_move(serializables->controlled_thing_idx, -serializables->mov_speed, 0);
                break;
            case 's':
                thing_move(serializables->controlled_thing_idx, 0, serializables->mov_speed);
                break;
            case 'd':
                thing_move(serializables->controlled_thing_idx, serializables->mov_speed, 0);
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
            case '=':
            case '+':
                platform->audio_set_volume(platform->audio_get_volume() + 0.1f);
                break;
            case '-':
            case '_':
                platform->audio_set_volume(platform->audio_get_volume() - 0.1f);
                break;
            default:
                break;
        }
    }
    while(input != '~');

}

static size_t load_texture_to_scratch(char *name)
{
    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Loading texture '%s' to scratch memory.", name);
    platform->debug_log(ephemerals->debug_buff);

    size_t index = ephemerals->scratch.used;
    Texture_t *texture_ptr = (Texture_t *)(ephemerals->scratch.buff+index);
    platform->gfx_load_texture(name, texture_ptr);
    size_t size = sizeof(Texture_t) + (texture_ptr->height * texture_ptr->width * texture_ptr->pixel_size_bytes);
    ephemerals->scratch.used += size;
    return index;
}

static size_t load_wav_to_scratch(char *name)
{
    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Loading wav '%s' to scratch memory.", name);
    platform->debug_log(ephemerals->debug_buff);

    size_t index = ephemerals->scratch.used;
    AudioClip_t *clip_ptr = (AudioClip_t *)(ephemerals->scratch.buff+index);
    platform->audio_load_wav(name, clip_ptr);
    size_t size = sizeof(AudioClip_t) + (sizeof(float) * clip_ptr->num_samples);
    ephemerals->scratch.used += size;
    return index;
}

static void audio_push_samples(float *samples, uint32_t len)
{
    if (app == NULL || app->audio_buffer == NULL) return;
    float_ring_push(app->audio_buffer, samples, len);
}

static void audio_push_clip(AudioClip_t *clip)
{
    audio_push_samples(clip->samples, clip->num_samples);
}

static int8_t things_compare_y(Thing_t *first, Thing_t *second)
{
    return (first->y_pos + first->y_offset) - (second->y_pos + second->y_offset);
}

static int8_t things_compare_layer(Thing_t *first, Thing_t *second)
{
    return first->layer - second->layer;
}

static void things_sort_by_comparer(uint16_t start_idx, uint16_t end_idx, int8_t (*comparer)(Thing_t*, Thing_t*))
{
    uint16_t temp;
    bool swapped = false;

    for (; end_idx >= start_idx; end_idx--)
    {
        if (!serializables->things[ephemerals->things_draw_order[end_idx]].used)
        {
            continue;
        }

        swapped = false;

        for (uint16_t i = start_idx; i < end_idx; i++)
        {
            if (comparer(
                        serializables->things+ephemerals->things_draw_order[i],
                        serializables->things+ephemerals->things_draw_order[i+1])
                        > 0)
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

static void things_initialize_draw_order(void)
{
    /// initialize unsorted state
    for (uint16_t i = 0; i < serializables->things_count; i++)
    {
        ephemerals->things_draw_order[i] = i;
    }

    things_sort_by_comparer(0, serializables->things_count-1, things_compare_layer);

    uint8_t layer = 0;
    Thing_t *thing;

    // default layer 0 offset before the actual work
    thing = serializables->things+ephemerals->things_draw_order[0];
    ephemerals->things_draw_order_layer_offsets[0] = thing->layer;

    for (uint16_t i = 1; i < serializables->things_count; i++)
    {
        thing = serializables->things+ephemerals->things_draw_order[i];

        if (thing->layer > layer)
        {
            layer = thing->layer;
            ephemerals->things_draw_order_layer_offsets[layer] = i;
        }
    }
}

static void things_update_draw_order(void)
{
    for (int8_t i = 0; i < APP_THINGS_LAYER_COUNT; i++)
    {
        things_sort_by_comparer(ephemerals->things_draw_order_layer_offsets[i],
                i == APP_THINGS_LAYER_COUNT - 1 ? serializables->things_count - 1
                : ephemerals->things_draw_order_layer_offsets[i+1],
                things_compare_y);
    }
}

static void things_get_distance(uint16_t first_thing_id, uint16_t second_thing_id, int32_t *x_dist_out, int32_t *y_dist_out)
{
    *x_dist_out = serializables->things[first_thing_id].x_pos - serializables->things[second_thing_id].x_pos;
    *y_dist_out = serializables->things[first_thing_id].y_pos - serializables->things[second_thing_id].y_pos;

    if (*x_dist_out < 0) *x_dist_out *= -1;
    if (*y_dist_out < 0) *y_dist_out *= -1;
}

static uint16_t thing_test_collision(uint16_t thing_id)
{
    int32_t dist_x;
    int32_t dist_y;

    for (uint16_t i = 0; i < serializables->things_count; i++)
    {
        if (i == thing_id) continue;

        things_get_distance(thing_id, i, &dist_x, &dist_y);

        if ((dist_x < (serializables->things[thing_id].coll_width + serializables->things[i].coll_width) / 2)
         && (dist_y < (serializables->things[thing_id].coll_height + serializables->things[i].coll_height) / 2))
        {
            return i;
        }
    }

    return thing_id;
}

static void thing_move(uint16_t thing_idx, int16_t x_delta, int16_t y_delta)
{
    serializables->things[thing_idx].x_pos += x_delta;
    serializables->things[thing_idx].y_pos += y_delta;

    if (thing_test_collision(thing_idx) != thing_idx)
    {
        serializables->things[thing_idx].x_pos -= x_delta;
        serializables->things[thing_idx].y_pos -= y_delta;
        return;
    }

    audio_push_clip((AudioClip_t *)(ephemerals->scratch.buff+serializables->things[thing_idx].move_sfx_idx));
    /*
    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Thing %d moved to [%d,%d]", thing_idx,
            serializables->things[thing_idx].x_pos, serializables->things[thing_idx].y_pos);
    platform->debug_log(ephemerals->debug_buff);
    */
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

static void gfx_draw_thing(uint16_t thing_idx)
{
    gfx_draw_texture((Texture_t *)(ephemerals->scratch.buff+serializables->things[thing_idx].texture_idx),
        serializables->things[thing_idx].x_pos - serializables->things[thing_idx].x_offset,
        serializables->things[thing_idx].y_pos - serializables->things[thing_idx].y_offset);
}

static void gfx_draw_all_things(void)
{
    for (uint16_t i = 0; i < serializables->things_count; i++)
    {
        gfx_draw_thing(ephemerals->things_draw_order[i]);
    }
}

/// must match prototype @ref AppSetupFunc
void app_setup(Platform_t *interface)
{
    static char debug_buff[128] = {0};

    platform = interface;

    platform->settings->gfx_frame_time_target_us = 16666;

    platform->settings->gfx_pixel_size_bytes = 1;
    platform->settings->gfx_buffer_width = 160;
    platform->settings->gfx_buffer_height = 32;

    size_t required_gfx_memory = 
    platform->settings->gfx_pixel_size_bytes *
    platform->settings->gfx_buffer_width *
    platform->settings->gfx_buffer_height;

    platform->settings->audio_buffer_capacity = 32768;

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
void app_init(const Platform_t *interface, AppMemoryPartition_t *memory)
{
    platform = interface;

    platform->debug_log("App initializing fresh memory partition.");

    if (memory == NULL)
    {
        platform->debug_log("Null memory partition pointer provided by platform.");
        platform->set_should_terminate(true);
        return;
    }

    app = memory;
    ephemerals = (AppEphemeral_t *)app->ephemeral->buffer;
    serializables = (AppSerializable_t *)app->serializable->buffer;

    /// EPHEMERALS
    platform->debug_log("Initializing app ephemeral state.");

    ephemerals->scratch.used = 0;

    /// SERIALIZABLES

    if (serializables->initialized)
    {
        platform->debug_log("Loaded serializable state already initialized.");
    }
    else
    {
        platform->debug_log("Initializing app serializable state.");

        serializables->things_count = APP_THINGS_COUNT;

        serializables->things[0].texture_idx = load_texture_to_scratch("thing1.bmp");
        serializables->things[0].move_sfx_idx = load_wav_to_scratch("sfx01.wav");
        serializables->things[0].x_pos = 24;
        serializables->things[0].y_pos = 8;
        serializables->things[0].x_offset = 4;
        serializables->things[0].y_offset = 6;
        serializables->things[0].coll_height = 1;
        serializables->things[0].coll_width = 5;
        serializables->things[0].layer = 5;
        serializables->things[0].used = true;

        serializables->things[1].texture_idx = load_texture_to_scratch("thing2.bmp");
        serializables->things[1].move_sfx_idx = load_wav_to_scratch("sfx02.wav");
        serializables->things[1].x_pos = 32;
        serializables->things[1].y_pos = 12;
        serializables->things[1].x_offset = 4;
        serializables->things[1].y_offset = 6;
        serializables->things[1].coll_height = 1;
        serializables->things[1].coll_width = 5;
        serializables->things[1].layer = 5;
        serializables->things[1].used = true;

        size_t pillar_texture_idx = load_texture_to_scratch("pillar_iso_hstretch.bmp");
        size_t back_wall_texture_idx = load_texture_to_scratch("wall_back_hstretch.bmp");
        size_t floor_texture_idx = load_texture_to_scratch("floor_hstretch.bmp");

        size_t current_idx = 2;
        size_t start_idx = current_idx;
        size_t target_idx = current_idx+24;

        for (; current_idx < target_idx; current_idx++)
        {
            serializables->things[current_idx].texture_idx = back_wall_texture_idx;
            serializables->things[current_idx].x_pos = 0 + ((current_idx-start_idx)*4);
            serializables->things[current_idx].y_pos = 0;
            serializables->things[current_idx].x_offset = 0;
            serializables->things[current_idx].y_offset = 0;
            serializables->things[current_idx].coll_height = 0;
            serializables->things[current_idx].coll_width = 0;
            serializables->things[current_idx].layer = 0;
            serializables->things[current_idx].used = true;
        }

        for (int y = 0; y < 6; y++)
        {
            start_idx = current_idx;
            target_idx = current_idx+24;

            for (; current_idx < target_idx; current_idx++)
            {
                serializables->things[current_idx].texture_idx = floor_texture_idx;
                serializables->things[current_idx].x_pos = 0 + ((current_idx-start_idx)*4);
                serializables->things[current_idx].y_pos = 5 + (4*y);
                serializables->things[current_idx].x_offset = 0;
                serializables->things[current_idx].y_offset = 0;
                serializables->things[current_idx].coll_height = 0;
                serializables->things[current_idx].coll_width = 0;
                serializables->things[current_idx].layer = 0;
                serializables->things[current_idx].used = true;
            }
        }

        start_idx = current_idx;
        target_idx = current_idx+7;

        for (; current_idx < target_idx; current_idx++)
        {
            serializables->things[current_idx].texture_idx = pillar_texture_idx;
            serializables->things[current_idx].x_pos = 5 + ((current_idx-start_idx)*16);
            serializables->things[current_idx].y_pos = 5;
            serializables->things[current_idx].x_offset = 3;
            serializables->things[current_idx].y_offset = 3;
            serializables->things[current_idx].coll_height = 2;
            serializables->things[current_idx].coll_width = 12;
            serializables->things[current_idx].layer = 5;
            serializables->things[current_idx].used = true;
        }

        serializables->controlled_thing_idx = 0;
        serializables->mov_speed = 1;

        serializables->initialized = true;
    }

    /// section combining serializables and ephemerals
    things_initialize_draw_order();
    things_update_draw_order();
}

/// must match prototype @ref AppLoopFunc
void app_loop(void)
{
    input_process_all();
    things_update_draw_order();
    gfx_clear_buffer();
    gfx_draw_all_things();
}

/// must match prototype @ref AppExitFunc
void app_exit(void)
{
}
