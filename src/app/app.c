#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "common_interface.h"
#include "common_structs.h"

#define APP_BUMP_SIZE (1024*2048)
#define APP_SCRATCH_SIZE (4096)

#define APP_TEXTURES_MAX_COUNT (64)
#define APP_SOUNDS_MAX_COUNT (64)

#define APP_LAYER_COUNT (6)
#define APP_PREFABS_MAX_COUNT (32)

#define SCENE_THINGS_MAX_COUNT (512)

typedef struct Prefab
{
    size_t texture_idx;
    size_t move_sfx_idx;
    int16_t x_offset;
    int16_t y_offset;
    uint16_t coll_width;
    uint16_t coll_height;
} Prefab_t;

typedef struct Thing
{
    bool used;
    uint16_t prefab_idx;
    int16_t x_pos;
    int16_t y_pos;
    uint8_t layer;
} Thing_t;

typedef struct AppSerializables
{
    bool initialized;
    uint16_t mov_speed;
    uint16_t things_count;
    uint16_t controlled_thing_idx;
    Thing_t things[SCENE_THINGS_MAX_COUNT];
} AppSerializable_t;

typedef struct AppEphemera
{
    char debug_buff[DEBUG_MESSAGE_MAX_LEN];
    uint8_t scratch[APP_SCRATCH_SIZE];

    uint16_t prefabs_count;
    size_t prefab_offsets[APP_PREFABS_MAX_COUNT];

    uint16_t sounds_count;
    size_t sound_offsets[APP_SOUNDS_MAX_COUNT];

    uint16_t textures_count;
    size_t texture_offsets[APP_TEXTURES_MAX_COUNT];

    int32_t things_draw_order_layer_offsets[APP_LAYER_COUNT];
    uint16_t things_draw_order[SCENE_THINGS_MAX_COUNT];

    size_t bump_used;
    uint8_t bump_buffer[APP_BUMP_SIZE];
} AppEphemeral_t;

static const Platform_t *platform = NULL;
static AppMemoryPartition_t *app = NULL;
static AppSerializable_t *serializables = NULL;
static AppEphemeral_t *ephemerals = NULL;

static char input_read_from_buffer(UniformRing_t *input_buffer);
static void input_process_all(void);

static size_t load_texture_to_memory(char *name);
static size_t load_wav_to_memory(char *name);

static void load_prefabs_all(void);

static void audio_push_samples(float *samples, uint32_t len, uint8_t channels);
static void audio_push_clip(AudioClip_t *clip);

static void things_initialize_draw_order(void);
static void things_update_draw_order(void);
static void things_get_distance(uint16_t first_thing_id, uint16_t second_thing_id, int32_t *x_dist_out, int32_t *y_dist_out);
static uint16_t thing_test_collision(uint16_t thing_id);
static void thing_move(uint16_t thing_idx, int16_t x_delta, int16_t y_delta);
static int32_t thing_create(uint8_t prefab, uint8_t layer, uint16_t x, uint16_t y);
static void thing_set_pos(uint32_t thing_id, int16_t x, int16_t y);
/*
static void thing_set_texture(uint32_t thing_id, uint32_t texture_id, int16_t x_offset, int16_t y_offset, int16_t coll_width, int16_t coll_height);
static void thing_set_move_sfx(uint32_t thing_id, uint32_t sfx_id);
*/
static int8_t things_compare_y(Thing_t *first, Thing_t *second);
static int8_t things_compare_layer(Thing_t *first, Thing_t *second);
static void things_sort_by_comparer(uint32_t start_idx, uint32_t end_idx, int8_t (*comparer)(Thing_t*, Thing_t*));

static void gfx_clear_buffer(void);
static void gfx_draw_texture(Texture_t *texture, int start_x, int start_y);
static void gfx_draw_thing(uint32_t thing_idx);
static void gfx_draw_all_things(void);

static char input_read_from_buffer(UniformRing_t *input_buffer)
{
    if (app == NULL || serializables == NULL) return '~';

    int c = '~';

    if (ring_pop(input_buffer, &c)) return c;
    return '~';
}

static void input_process_all(void)
{
    if (platform == NULL || serializables == NULL) return;

    int input = '~';

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

static size_t load_texture_to_memory(char *name)
{
    size_t index = ephemerals->bump_used;
    Texture_t *texture_ptr = (Texture_t *)(ephemerals->bump_buffer+index);

    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff),
            "Loading texture '%s' to scratch memory at offset %ld, address %p.",
            name, index, (void *)texture_ptr);
    platform->debug_log(ephemerals->debug_buff);

    platform->gfx_load_texture(name, texture_ptr);
    size_t size = sizeof(Texture_t) + (texture_ptr->height * texture_ptr->width * texture_ptr->pixel_size_bytes);
    ephemerals->bump_used += size;

    ephemerals->texture_offsets[ephemerals->textures_count] = index;
    ephemerals->textures_count++;

    return index;
}

static size_t load_wav_to_memory(char *name)
{
    size_t index = ephemerals->bump_used;
    AudioClip_t *clip_ptr = (AudioClip_t *)(ephemerals->bump_buffer+index);

    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Loading wav '%s' to scratch memory at offset %lu.", name, index);
    platform->debug_log(ephemerals->debug_buff);

    platform->audio_load_wav(name, clip_ptr);

    /// TODO: finish implementing trim_silence
    /*
    bool trim_silence = true;

    if (trim_silence)
    {
        size_t end_idx = clip_ptr->num_samples - 1;

        while(end_idx > 3
            && clip_ptr->samples[end_idx] == clip_ptr->samples[end_idx-2]
            && clip_ptr->samples[end_idx-1] == clip_ptr->samples[end_idx-3])
        {
            clip_ptr->num_samples -= 2;
            end_idx = clip_ptr->num_samples - 1;
        }

        snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Trimmed wav '%s', final size: %lu.",
                name, sizeof(AudioClip_t) + (sizeof(float) * clip_ptr->num_samples));
        platform->debug_log(ephemerals->debug_buff);
    }
    */

    size_t size = sizeof(AudioClip_t) + (sizeof(float) * clip_ptr->num_samples);
    ephemerals->bump_used += size;
    
    ephemerals->sound_offsets[ephemerals->sounds_count] = index;
    ephemerals->sounds_count++;

    return index;
}

static void load_prefabs_all(void)
{
    bzero(ephemerals->scratch, APP_SCRATCH_SIZE);
    size_t txt_len = platform->storage_load_text("prefabs.soft", (char *)ephemerals->scratch, APP_SCRATCH_SIZE);

    if (txt_len > 0)
    {
        char *current_line = (char *)ephemerals->scratch;
        char *next_line = NULL;

        uint16_t current_idx = 0;
        Prefab_t *current_ptr = NULL;
        uint8_t state = 0; // 0 texture, 1 sfx, 2 offsets, 3 coll
        char *token = NULL;

        while (current_line != NULL && ephemerals->textures_count < APP_SOUNDS_MAX_COUNT)
        {
            next_line = strchr(current_line, '\n');
            if (next_line) *next_line = '\0';
            
            if (strlen(current_line) > 0
                && current_line[0] != '#')
            {
                switch (state)
                {
                    case 0:
                        /// base prefab idx on prefab count
                        current_idx = ephemerals->prefabs_count;
                        /// increment prefab count
                        ephemerals->prefabs_count++;

                        /// base prefab offset on top of memory bump
                        ephemerals->prefab_offsets[current_idx]
                            = ephemerals->bump_used;
                        /// raise top of memory bump by size of prefab struct
                        ephemerals->bump_used += sizeof(Prefab_t);

                        /// set prefab pointer to the just established location
                        current_ptr = (Prefab_t *)(ephemerals->bump_buffer
                            + ephemerals->prefab_offsets[current_idx]);

                        /// set prefab texture idx accoring to line
                        current_ptr->texture_idx = atoi(current_line);

                        /// set state to read sfx next
                        state = 1;
                        break;
                    case 1:
                        /// set prefab move sfx idx accoring to line
                        current_ptr->move_sfx_idx = atoi(current_line);
                        /// set state to read offsets next
                        state = 2;
                        break;
                    case 2:
                        /// split line between x y according to delimiter
                        token = strtok(current_line, " ");
                        current_ptr->x_offset = atoi(token);
                        token = strtok(NULL, " ");
                        current_ptr->y_offset = atoi(token);
                        /// set state to read coll bounds next
                        state = 3;
                        break;
                    case 3:
                        /// split line between width height according to delimiter
                        token = strtok(current_line, " ");
                        current_ptr->coll_width = atoi(token);
                        token = strtok(NULL, " ");
                        current_ptr->coll_height = atoi(token);
                        /// set state to read next prefab texture idx next
                        state = 0;
                        current_ptr = NULL;
                        token = NULL;
                        break;
                }
            }

            current_line = next_line ? next_line + 1 : NULL;
        }
    }
}

static void load_scene_by_path(char *path)
{
    bzero(ephemerals->scratch, APP_SCRATCH_SIZE);
    size_t txt_len = platform->storage_load_text(path, (char *)ephemerals->scratch, APP_SCRATCH_SIZE);

    if (txt_len > 0)
    {
        char *current_line = (char *)ephemerals->scratch;
        char *next_line = NULL;

        uint8_t state = 0; // 0 prefab, 1 repetition, 2 layer, 3 x y per rep
        char *token = NULL;

        uint16_t prefab_index = 0;
        uint16_t rep_target = 0;
        uint16_t rep_counter = 0;
        uint16_t layer_index = 0;
        uint16_t x_pos = 0;
        uint16_t y_pos = 0;

        while (current_line != NULL && ephemerals->textures_count < APP_SOUNDS_MAX_COUNT)
        {
            next_line = strchr(current_line, '\n');
            if (next_line) *next_line = '\0';
            
            if (strlen(current_line) > 0
                && current_line[0] != '#')
            {
                switch (state)
                {
                    case 0:
                        /// set prefab idx according to line
                        prefab_index = atoi(current_line);
                        /// set state to read reps next
                        state = 1;
                        break;
                    case 1:
                        /// set rep target accoring to line
                        rep_target = atoi(current_line);
                        rep_counter = 0;
                        /// set state to read layer next
                        state = rep_target > 0 ? 2 : 0;
                        break;
                    case 2:
                        /// set layer according to line
                        layer_index = atoi(current_line);
                        /// set state to read next prefab texture idx next
                        state = 3;
                        break;
                    case 3:
                        /// split line between x y according to delimiter
                        token = strtok(current_line, " ");
                        x_pos = atoi(token);
                        token = strtok(NULL, " ");
                        y_pos = atoi(token);
                        thing_create(prefab_index, layer_index, x_pos, y_pos);

                        rep_counter++;

                        if (rep_counter >= rep_target)
                        {
                            /// set state to read next group of things
                            state = 0;
                            token = NULL;
                        }
                        break;
                }
            }

            current_line = next_line ? next_line + 1 : NULL;
        }

        snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff),
                "Loaded scene [%s], thing count: %u", path, serializables->things_count);
        platform->debug_log(ephemerals->debug_buff);
    }
    else
    {
        snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff),
                "Could not load scene [%s].", path);
        platform->debug_log(ephemerals->debug_buff);
    }
}

static void load_scene_by_index(uint8_t index)
{
    char path[256] = {0};
    bool found = false;

    bzero(ephemerals->scratch, APP_SCRATCH_SIZE);
    size_t txt_len = platform->storage_load_text("scenes.soft", (char *)ephemerals->scratch, APP_SCRATCH_SIZE);

    if (txt_len > 0)
    {
        char *current_line = (char *)ephemerals->scratch;
        char *next_line = NULL;
        uint8_t current_scene_index = 0;

        while (current_line != NULL && ephemerals->textures_count < APP_SOUNDS_MAX_COUNT)
        {
            next_line = strchr(current_line, '\n');
            if (next_line) *next_line = '\0';
            
            if (strlen(current_line) > 0
                && current_line[0] != '#')
            {
                if (current_scene_index == index)
                {
                    strncpy(path, current_line, sizeof(path));
                    found = true;
                    break;
                }
                current_scene_index++;
            }

            current_line = next_line ? next_line + 1 : NULL;
        }

        if (found)
        {
            load_scene_by_path(path);
        }
    }
}

static void audio_push_samples(float *samples, uint32_t len, uint8_t channels)
{
    if (app == NULL || app->audio_buffer == NULL) return;

    if (channels == platform->settings->audio_channels)
    {
        ring_push(app->audio_buffer, samples, len, false);
    }
    else
    {
        for (uint32_t i = 0; i < len; i+= channels)
        {
            for (uint8_t j = 0; j < platform->settings->audio_channels; j++)
            {
                ring_push(app->audio_buffer, samples+i+j, 1, false);
            }
        }
    }
}

static void audio_push_clip(AudioClip_t *clip)
{
    audio_push_samples(clip->samples, clip->num_samples, clip->num_channels);
}

static int32_t thing_create(uint8_t prefab, uint8_t layer, uint16_t x, uint16_t y)
{
    if (serializables->things_count >= SCENE_THINGS_MAX_COUNT)
    {
        platform->debug_log("Cannot create new Thing, reached maximum.");
        return -1;
    }

    uint16_t index = serializables->things_count;

    serializables->things[index].prefab_idx = prefab;
    serializables->things[index].layer = layer;
    serializables->things[index].x_pos = x;
    serializables->things[index].y_pos = y;
    serializables->things[index].used = true;

    serializables->things_count++;

    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Created new %u-Thing at index %u, [%u,%u] layer %u.", prefab, index, x, y, layer);
    platform->debug_log(ephemerals->debug_buff);

    return index;
}

static void thing_set_pos(uint32_t thing_id, int16_t x, int16_t y)
{
    serializables->things[thing_id].x_pos = x;
    serializables->things[thing_id].y_pos = y;

    /*
    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Set Thing %u at position [%u,%u].", thing_id, x, y);
    platform->debug_log(ephemerals->debug_buff);
    */
}

/*
static void thing_set_texture(uint32_t thing_id, uint32_t texture_id, int16_t x_offset, int16_t y_offset, int16_t coll_width, int16_t coll_height)
{
    serializables->things[thing_id].texture_idx = texture_id;
    serializables->things[thing_id].x_offset = x_offset;
    serializables->things[thing_id].x_offset = y_offset;
    serializables->things[thing_id].coll_width = coll_width;
    serializables->things[thing_id].coll_height = coll_height;

}

static void thing_set_move_sfx(uint32_t thing_id, uint32_t sfx_id)
{
    serializables->things[thing_id].move_sfx_idx = sfx_id;
}
*/

static Prefab_t* thing_get_prefab(Thing_t *thing)
{
    return ((Prefab_t *)(ephemerals->bump_buffer+ephemerals->prefab_offsets[thing->prefab_idx]));
}

static int8_t things_compare_y(Thing_t *first, Thing_t *second)
{
    return (first->y_pos + thing_get_prefab(first)->y_offset) - (second->y_pos + thing_get_prefab(second)->y_offset);
}

static int8_t things_compare_layer(Thing_t *first, Thing_t *second)
{
    return first->layer - second->layer;
}

static void things_sort_by_comparer(uint32_t start_idx, uint32_t end_idx, int8_t (*comparer)(Thing_t*, Thing_t*))
{
    uint16_t temp;
    bool swapped = false;

    for (; end_idx > start_idx; end_idx--)
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
    if (serializables->things_count <= 0)
    {
        platform->debug_log("Cannot initialize draw order - thing count is zero!");
        return;
    }

    /// initialize unsorted state
    for (uint16_t i = 0; i < serializables->things_count; i++)
    {
        ephemerals->things_draw_order[i] = i;
    }

    /// initialize layer offsets to -1, e.g. no things using that layer
    for (uint8_t i = 0; i < APP_LAYER_COUNT; i++)
    {
        ephemerals->things_draw_order_layer_offsets[i] = -1;
    }

    things_sort_by_comparer(0, serializables->things_count-1, things_compare_layer);

    uint8_t layer = 0;
    Thing_t *thing;

    // default layer 0 offset before the actual work
    thing = serializables->things+ephemerals->things_draw_order[0];
    layer = thing->layer;
    ephemerals->things_draw_order_layer_offsets[thing->layer] = 0;

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
    if (serializables->things_count <= 0)
    {
        platform->debug_log("Cannot update draw order - thing count is zero!");
        return;
    }

    int32_t start_idx = 0;
    int32_t end_idx = 0;

    for (int8_t i = 0; i < APP_LAYER_COUNT; i++)
    {
        start_idx = ephemerals->things_draw_order_layer_offsets[i];

        if (start_idx < 0) continue;

        end_idx = -1;

        for (int8_t j = i+1; j < APP_LAYER_COUNT; j++)
        {
            if (ephemerals->things_draw_order_layer_offsets[j] > start_idx)
            {
                end_idx = ephemerals->things_draw_order_layer_offsets[j] - 1;
                break;
            }
        }

        if (end_idx < 0)
        {
            things_sort_by_comparer(start_idx, serializables->things_count - 1, things_compare_y);
            break;
        }

        things_sort_by_comparer(start_idx, end_idx, things_compare_y);
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
    uint16_t width = thing_get_prefab(&serializables->things[thing_id])->coll_width;
    uint16_t height = thing_get_prefab(&serializables->things[thing_id])->coll_height;

    if (width == 0 || height == 0) return thing_id;

    for (uint16_t i = 0; i < serializables->things_count; i++)
    {
        if (i == thing_id) continue;
        uint16_t other_width = thing_get_prefab(&serializables->things[i])->coll_width;
        uint16_t other_height = thing_get_prefab(&serializables->things[i])->coll_height;
        if (other_width == 0 || other_height == 0) continue;

        things_get_distance(thing_id, i, &dist_x, &dist_y);

        if ((dist_x < (width + other_width) / 2)
         && (dist_y < (height + other_height) / 2))
        {
            snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Thing %d collided with Thing %d.", thing_id, i);
            platform->debug_log(ephemerals->debug_buff);
            return i;
        }
    }

    return thing_id;
}

static void thing_move(uint16_t thing_idx, int16_t x_delta, int16_t y_delta)
{
    if (!serializables->things[thing_idx].used) return;

    serializables->things[thing_idx].x_pos += x_delta;
    serializables->things[thing_idx].y_pos += y_delta;

    if (thing_test_collision(thing_idx) != thing_idx)
    {
        serializables->things[thing_idx].x_pos -= x_delta;
        serializables->things[thing_idx].y_pos -= y_delta;
        return;
    }

    size_t sfx_offset = ephemerals->sound_offsets[thing_get_prefab(&serializables->things[thing_idx])->move_sfx_idx];
    audio_push_clip((AudioClip_t *)(ephemerals->bump_buffer+sfx_offset));
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

static void gfx_draw_thing(uint32_t thing_idx)
{
    if (!serializables->things[thing_idx].used) return;
    size_t texture_offset = ephemerals->texture_offsets[thing_get_prefab(&serializables->things[thing_idx])->texture_idx];
    gfx_draw_texture((Texture_t *)(ephemerals->bump_buffer+texture_offset),
        serializables->things[thing_idx].x_pos - thing_get_prefab(&serializables->things[thing_idx])->x_offset,
        serializables->things[thing_idx].y_pos - thing_get_prefab(&serializables->things[thing_idx])->y_offset);
}

static void gfx_draw_all_things_debug(void)
{
    static uint16_t step = 0;
    static uint16_t counter = 0;

    Thing_t *step_thing = serializables->things+ephemerals->things_draw_order[step];
    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff),
            "Thing %u, ypos %u, layer %u.", ephemerals->things_draw_order[step], step_thing->y_pos, step_thing->layer);
    platform->debug_log(ephemerals->debug_buff);

    for (uint16_t i = 0; i < step; i++)
    {
        gfx_draw_thing(ephemerals->things_draw_order[i]);
    }

    if (counter >= 4)
    {
        counter = 0;
        step++;
        if (step >= serializables->things_count) step = 0;
    }
    else
    {
        counter++;
    }
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
    platform->settings->gfx_buffer_width = 148;
    platform->settings->gfx_buffer_height = 32;

    size_t required_gfx_memory = 
    platform->settings->gfx_pixel_size_bytes *
    platform->settings->gfx_buffer_width *
    platform->settings->gfx_buffer_height;

    platform->settings->audio_channels = 2;
    platform->settings->audio_buffer_capacity = 32768;

    platform->settings->input_buffer_capacity = 128;

    size_t required_state_memory = sizeof(AppSerializable_t) + sizeof(AppEphemeral_t);

    size_t required_memory_total = required_state_memory + required_gfx_memory
        + sizeof(UniformRing_t) + (sizeof(float) * platform->settings->audio_buffer_capacity)
        + sizeof(UniformRing_t) + (sizeof(int) * platform->settings->input_buffer_capacity);
    
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
    ephemerals->bump_used = 0;

    /// load all textures according to file
    size_t txt_len = 0;

    bzero(ephemerals->scratch, APP_SCRATCH_SIZE);
    txt_len = platform->storage_load_text("textures.soft", (char *)ephemerals->scratch, APP_SCRATCH_SIZE);

    if (txt_len > 0)
    {
        char *current_line = (char *)ephemerals->scratch;
        char *next_line = NULL;

        while (current_line != NULL && ephemerals->textures_count < APP_TEXTURES_MAX_COUNT)
        {
            next_line = strchr(current_line, '\n');
            if (next_line) *next_line = '\0';
            
            if (strlen(current_line) > 5)
            {
                load_texture_to_memory(current_line);
            }

            //if (next_line) *next_line = '\n';
            current_line = next_line ? next_line + 1 : NULL;
        }
    }

    /// load all sounds according to file
    bzero(ephemerals->scratch, APP_SCRATCH_SIZE);
    txt_len = platform->storage_load_text("sounds.soft", (char *)ephemerals->scratch, APP_SCRATCH_SIZE);

    if (txt_len > 0)
    {
        char *current_line = (char *)ephemerals->scratch;
        char *next_line = NULL;

        while (current_line != NULL && ephemerals->textures_count < APP_SOUNDS_MAX_COUNT)
        {
            next_line = strchr(current_line, '\n');
            if (next_line) *next_line = '\0';
            
            if (strlen(current_line) > 5)
            {
                load_wav_to_memory(current_line);
            }

            //if (next_line) *next_line = '\n';
            current_line = next_line ? next_line + 1 : NULL;
        }
    }

    /// load all prefabs according to file
    load_prefabs_all();

    /// SERIALIZABLES

    if (serializables->initialized)
    {
        platform->debug_log("Loaded serializable state already initialized.");
    }
    else
    {
        platform->debug_log("Initializing app serializable state.");

        serializables->things_count = 0;
        explicit_bzero(serializables->things, sizeof(Thing_t) * SCENE_THINGS_MAX_COUNT);

        /*
        size_t thing1_texture_idx = load_texture_to_memory("textures/thing1.bmp");
        size_t thing2_texture_idx = load_texture_to_memory("textures/thing2.bmp");
        size_t pillar_texture_idx = load_texture_to_memory("textures/pillar_iso_hstretch.bmp");
        size_t back_wall_texture_idx = load_texture_to_memory("textures/wall_back_hstretch.bmp");
        size_t floor_texture_idx = load_texture_to_memory("textures/floor_hstretch.bmp");
        size_t sfx01_idx = load_wav_to_memory("sfx/sfx01.wav");
        size_t sfx02_idx = load_wav_to_memory("sfx/sfx02.wav");
        */

        /*
        uint16_t thing_idx;

        for (int i = 0; i < 2; i++)
        {
            thing_idx = thing_create(5);

            thing_set_texture(thing_idx, i == 0 ? ephemerals->texture_offsets[0] : ephemerals->texture_offsets[1], 4, 6, 6, 2);
            thing_set_pos(thing_idx, 24*(i+1), 8);

            thing_set_move_sfx(thing_idx, i == 0 ? ephemerals->sound_offsets[0] : ephemerals->sound_offsets[1]);
        }

        for (int i = 0; i < 7; i++)
        {
            thing_idx = thing_create(5);

            thing_set_texture(thing_idx, ephemerals->texture_offsets[2], 6, 3, 12, 2);
            thing_set_pos(thing_idx, 5 + (i*16), 5);
        }

        for (int i = 0; i < 16; i++)
        {
            thing_idx = thing_create(0);

            thing_set_texture(thing_idx, ephemerals->texture_offsets[3], 0, 0, 0, 0);
            thing_set_pos(thing_idx, 0 + (i*8), 0);
        }

        for (int y = 0; y < 3; y++)
        {
            for (int i = 0; i < 12; i++)
            {
                thing_idx = thing_create(0);

                thing_set_texture(thing_idx, ephemerals->texture_offsets[4], 0, 0, 0, 0);
                thing_set_pos(thing_idx, 0 + (i*8), 5 + (8 * y));
            }
        }
        */
        load_scene_by_index(0);

        serializables->controlled_thing_idx = 0;
        serializables->mov_speed = 1;

        serializables->initialized = true;
    }

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
    //gfx_draw_all_things_debug();
}

/// must match prototype @ref AppExitFunc
void app_exit(void)
{
}
