#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "common_interface.h"
#include "common_structs.h"

#include "app_thing.h"
#include "app_scene.h"

#define APP_BUMP_SIZE (1024*2048)
#define APP_SCRATCH_SIZE (4096)

#define APP_TEXTURES_MAX_COUNT (64)
#define APP_SOUNDS_MAX_COUNT (64)

#define APP_LAYER_COUNT (6)
#define APP_PREFABS_MAX_COUNT (32)

#define APP_MAX_SCENES (16)

typedef struct AppSerializables
{
    bool initialized;
    uint16_t mov_speed;
    uint16_t controlled_thing_idx;
    uint16_t focal_thing_idx;
    uint8_t scene_count;
    uint8_t scene_index;
    Scene_t scenes[APP_MAX_SCENES];
} AppSerializable_t;

typedef struct AppEphemera
{
    char debug_buff[DEBUG_MESSAGE_MAX_LEN];
    uint8_t scratch[APP_SCRATCH_SIZE];

    uint16_t prefabs_count;
    Prefab_t prefabs[APP_PREFABS_MAX_COUNT];
    Sprite_t sprites[APP_PREFABS_MAX_COUNT];
    Collider_t colliders[APP_PREFABS_MAX_COUNT];
    SoundEmitter_t sound_emitters[APP_PREFABS_MAX_COUNT];

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

static bool debug_gfx = false;

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

static void load_ephemerals(void);

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
                /// DEBUG KEYS
            case '0':
                debug_gfx = !debug_gfx;
                break;
            case '9':
                load_ephemerals();
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
        ephemerals->prefabs_count = 0;

        char *current_line = (char *)ephemerals->scratch;
        char *next_line = NULL;

        uint16_t current_idx = 0;
        ThingFlags_t current_flags = 0;
        uint8_t state = 0; // 0 flags, 1 texture id & offsets, 2 coll bounds, 3 coll flags, 4 sfx
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
                        // read flags from line and decide next state
                        current_flags = atoi(current_line);
                        ephemerals->prefabs[current_idx].flags = current_flags;
                        state = current_flags & THING_FLAGS_TEXTURE ? 1
                              : current_flags & THING_FLAGS_COLLISION ? 2
                              : current_flags & THING_FLAGS_SOUND ? 4
                              : 0;
                        break;
                    case 1:
                        /// split line between id, x & y according to delimiter
                        /// and set texture id & offsets accordingly
                        token = strtok(current_line, " ");
                        ephemerals->sprites[current_idx].texture_idx = atoi(token);
                        token = strtok(NULL, " ");
                        ephemerals->sprites[current_idx].x_offset = atoi(token);
                        token = strtok(NULL, " ");
                        ephemerals->sprites[current_idx].y_offset = atoi(token);
                        /// set next state according to flags
                        state = current_flags & THING_FLAGS_COLLISION ? 2
                              : current_flags & THING_FLAGS_SOUND ? 4
                              : 0;
                        break;
                    case 2:
                        /// split line between min x / min y / max x / max y, according to delimiter
                        token = strtok(current_line, " ");
                        ephemerals->colliders[current_idx].min_x = atoi(token);
                        token = strtok(NULL, " ");
                        ephemerals->colliders[current_idx].min_y = atoi(token);
                        token = strtok(NULL, " ");
                        ephemerals->colliders[current_idx].max_x = atoi(token);
                        token = strtok(NULL, " ");
                        ephemerals->colliders[current_idx].max_y = atoi(token);

                        state = 3;
                        break;
                    case 3:
                        /// split line between flags + 5 params, according to delimiter
                        token = strtok(current_line, " ");
                        ephemerals->colliders[current_idx].flags = atoi(token);
                        for (uint8_t i = 0; i < 5; i++)
                        {
                            token = strtok(NULL, " ");
                            ephemerals->colliders[current_idx].params[i] = atoi(token);
                        }
                        /// set next state according to flags
                        state = current_flags & THING_FLAGS_SOUND ? 4
                              : 0;
                        break;
                    case 4:
                        /// set prefab move sfx idx accoring to line
                        ephemerals->sound_emitters[current_idx].move_sfx_idx = atoi(current_line);
                        /// set state to read offsets next
                        state = 0;
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

        serializables->scenes[serializables->scene_index].loaded = true;

        snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff),
                "Loaded scene [%s], thing count: %u", path, serializables->scenes[serializables->scene_index].things_count);
        platform->debug_log(ephemerals->debug_buff);

        things_initialize_draw_order();
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
    /// if scene was loaded before, simply set it to be the current scene
    if (serializables->scenes[index].loaded)
    {
        serializables->scene_index = index;
        things_initialize_draw_order();
        return;
    }

    /// else, attempt to load fresh from file
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
            serializables->scene_index = index;
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
    uint16_t index = serializables->scenes[serializables->scene_index].things_count;

    if (index >= SCENE_THINGS_MAX_COUNT)
    {
        platform->debug_log("Cannot create new Thing, reached maximum.");
        return -1;
    }

    serializables->scenes[serializables->scene_index].things_count++;

    Thing_t *thing = &serializables->scenes[serializables->scene_index].things[index];

    thing->used = true;
    thing->layer = layer;
    thing->prefab_idx = prefab;

    thing->transform.x_pos = x;
    thing->transform.y_pos = y;

    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Created new %u-Thing at index %u, [%u,%u] layer %u.", prefab, index, x, y, layer);
    platform->debug_log(ephemerals->debug_buff);

    return index;
}

static void thing_set_pos(uint32_t thing_id, int16_t x, int16_t y)
{
    serializables->scenes[serializables->scene_index].things[thing_id].transform.x_pos = x;
    serializables->scenes[serializables->scene_index].things[thing_id].transform.y_pos = y;

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
    return &ephemerals->prefabs[thing->prefab_idx];
}

static Sprite_t* thing_get_sprite(Thing_t *thing)
{
    return &ephemerals->sprites[thing->prefab_idx];
}

static Collider_t* thing_get_collider(Thing_t *thing)
{
    return &ephemerals->colliders[thing->prefab_idx];
}

static SoundEmitter_t* thing_get_sounds(Thing_t *thing)
{
    return &ephemerals->sound_emitters[thing->prefab_idx];
}

static int8_t things_compare_y(Thing_t *first, Thing_t *second)
{
    return (first->transform.y_pos + thing_get_sprite(first)->y_offset) - (second->transform.y_pos + thing_get_sprite(second)->y_offset);
}

static int8_t things_compare_layer(Thing_t *first, Thing_t *second)
{
    return first->layer - second->layer;
}

static void things_sort_by_comparer(uint32_t start_idx, uint32_t end_idx, int8_t (*comparer)(Thing_t*, Thing_t*))
{
    uint16_t temp;
    bool swapped = false;

    Scene_t *scene = &serializables->scenes[serializables->scene_index];

    for (; end_idx > start_idx; end_idx--)
    {
        if (!scene->things[ephemerals->things_draw_order[end_idx]].used)
        {
            continue;
        }

        swapped = false;

        for (uint16_t i = start_idx; i < end_idx; i++)
        {
            if (comparer(
                        scene->things+ephemerals->things_draw_order[i],
                        scene->things+ephemerals->things_draw_order[i+1])
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
    Scene_t *scene = &serializables->scenes[serializables->scene_index];

    if (scene->things_count <= 0)
    {
        platform->debug_log("Cannot initialize draw order - thing count is zero!");
        return;
    }

    /// initialize unsorted state
    for (uint16_t i = 0; i < scene->things_count; i++)
    {
        ephemerals->things_draw_order[i] = i;
    }

    /// initialize layer offsets to -1, e.g. no things using that layer
    for (uint8_t i = 0; i < APP_LAYER_COUNT; i++)
    {
        ephemerals->things_draw_order_layer_offsets[i] = -1;
    }

    things_sort_by_comparer(0, scene->things_count-1, things_compare_layer);

    uint8_t layer = 0;
    Thing_t *thing;

    // default layer 0 offset before the actual work
    thing = scene->things+ephemerals->things_draw_order[0];
    layer = thing->layer;
    ephemerals->things_draw_order_layer_offsets[thing->layer] = 0;

    for (uint16_t i = 1; i < scene->things_count; i++)
    {
        thing = scene->things+ephemerals->things_draw_order[i];

        if (thing->layer > layer)
        {
            layer = thing->layer;
            ephemerals->things_draw_order_layer_offsets[layer] = i;
        }
    }
}

static void things_update_draw_order(void)
{
    Scene_t *scene = &serializables->scenes[serializables->scene_index];

    if (scene->things_count <= 0)
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
            things_sort_by_comparer(start_idx, scene->things_count - 1, things_compare_y);
            break;
        }

        things_sort_by_comparer(start_idx, end_idx, things_compare_y);
    }
}

static void things_get_distance(uint16_t first_thing_id, uint16_t second_thing_id, int32_t *x_dist_out, int32_t *y_dist_out)
{
    Scene_t *scene = &serializables->scenes[serializables->scene_index];

    *x_dist_out = scene->things[first_thing_id].transform.x_pos - scene->things[second_thing_id].transform.x_pos;
    *y_dist_out = scene->things[first_thing_id].transform.y_pos - scene->things[second_thing_id].transform.y_pos;

    if (*x_dist_out < 0) *x_dist_out *= -1;
    if (*y_dist_out < 0) *y_dist_out *= -1;
}

static uint16_t thing_test_collision(uint16_t thing_id)
{
    Scene_t *scene = &serializables->scenes[serializables->scene_index];
    if (!(ephemerals->prefabs[scene->things[thing_id].prefab_idx].flags & THING_FLAGS_COLLISION)) return thing_id;

    Transform_t *thing_transform = &scene->things[thing_id].transform;
    Collider_t *thing_collider = &ephemerals->colliders[scene->things[thing_id].prefab_idx];

    Transform_t *other_transform = NULL;
    Collider_t *other_collider = NULL;

    int16_t thing_left =   thing_transform->x_pos + thing_collider->min_x;
    int16_t thing_right =  thing_transform->x_pos + thing_collider->max_x;
    int16_t thing_top =    thing_transform->y_pos + thing_collider->min_y;
    int16_t thing_bottom = thing_transform->y_pos + thing_collider->max_y;

    int16_t other_left = 0;
    int16_t other_right = 0;
    int16_t other_top = 0;
    int16_t other_bottom = 0;

    for (uint16_t i = 0; i < scene->things_count; i++)
    {
        /// avoid testing against self
        if (i == thing_id) continue;
        /// avoid testing against non-collider
        if (!(ephemerals->prefabs[scene->things[i].prefab_idx].flags & THING_FLAGS_COLLISION)) continue;

        other_transform = &scene->things[i].transform;
        other_collider = &ephemerals->colliders[scene->things[i].prefab_idx];

        other_left =   other_transform->x_pos + other_collider->min_x;
        other_right =  other_transform->x_pos + other_collider->max_x;
        other_top =    other_transform->y_pos + other_collider->min_y;
        other_bottom = other_transform->y_pos + other_collider->max_y;

        if(thing_right >= other_left
        && thing_left <= other_right
        && thing_top <= other_bottom
        && thing_bottom >= other_top)
        {
            snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Thing %d collided with Thing %d.", thing_id, i);
            platform->debug_log(ephemerals->debug_buff);
            return i;
        }
    }

    return thing_id;
}

static void collision_process(uint16_t thing_idx, uint16_t other_idx)
{
}

static void thing_move(uint16_t thing_idx, int16_t x_delta, int16_t y_delta)
{
    Scene_t *scene = &serializables->scenes[serializables->scene_index];

    if (!scene->things[thing_idx].used) return;

    scene->things[thing_idx].transform.x_pos += x_delta;
    scene->things[thing_idx].transform.y_pos += y_delta;

    uint16_t other_idx = thing_test_collision(thing_idx);

    if (other_idx != thing_idx)
    {
        /*
    COLL_FLAGS_NONE = 0x00,
    COLL_FLAGS_BLOCK = 0x01,
    COLL_FLAGS_PLAY_SOUND = 0x02,
    COLL_FLAGS_SET_SCENE = 0x04,
    COLL_FLAGS_SET_POSITION = 0x08,
    COLL_FLAGS_CALLBACK = 0x10,
    */
        Collider_t * other_coll = &ephemerals->colliders[scene->things[other_idx].prefab_idx];

        if (other_coll->flags & COLL_FLAGS_BLOCK)
        {
            scene->things[thing_idx].transform.x_pos -= x_delta;
            scene->things[thing_idx].transform.y_pos -= y_delta;
        }

        if (other_coll->flags & COLL_FLAGS_PLAY_SOUND)
        {
            audio_push_clip((AudioClip_t *)(ephemerals->bump_buffer+ephemerals->sound_offsets[other_coll->params[0]]));
        }

        if ((other_coll->flags & COLL_FLAGS_SET_SCENE)
            && thing_idx == serializables->controlled_thing_idx)
        {
            load_scene_by_index(other_coll->params[1]);
            /// TODO: this is obviously a bad way to do it and needs to go later
            //thing_idx = serializables->controlled_thing_idx;
        }

        if (other_coll->flags & COLL_FLAGS_SET_POSITION)
        {
            scene->things[thing_idx].transform.x_pos = other_coll->params[2];
            scene->things[thing_idx].transform.x_pos = other_coll->params[3];
        }

        if (other_coll->flags & COLL_FLAGS_CALLBACK)
        {
            /// TODO: add collision callback array
        }

        return;
    }

    size_t sfx_offset = ephemerals->sound_offsets[thing_get_sounds(&scene->things[thing_idx])->move_sfx_idx];
    audio_push_clip((AudioClip_t *)(ephemerals->bump_buffer+sfx_offset));
    /*
    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Thing %d moved to [%d,%d]", thing_idx,
            serializables->things[thing_idx].x_pos, serializables->things[thing_idx].y_pos);
    platform->debug_log(ephemerals->debug_buff);
    */
}

static void gfx_world_to_screen_coords(int16_t *x_ptr, int16_t *y_ptr)
{
    Scene_t *scene = &serializables->scenes[serializables->scene_index];

    *x_ptr += app->gfx_buffer->width/2;
    *x_ptr -= scene->things[serializables->focal_thing_idx].transform.x_pos;
    *y_ptr += app->gfx_buffer->height/2;
    *y_ptr -= scene->things[serializables->focal_thing_idx].transform.y_pos;
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

static void gfx_debug_draw_collider(uint32_t thing_idx, uint32_t draw_val)
{
    if (app == NULL || app->gfx_buffer == NULL) return;

    Scene_t *scene = &serializables->scenes[serializables->scene_index];

    if (!scene->things[thing_idx].used) return;
    if (!(ephemerals->prefabs[scene->things[thing_idx].prefab_idx].flags & THING_FLAGS_COLLISION)) return;

    Transform_t *transform = &scene->things[thing_idx].transform;
    Collider_t *collider = &ephemerals->colliders[scene->things[thing_idx].prefab_idx];

    int16_t min_x = collider->min_x + transform->x_pos;
    int16_t min_y = collider->min_y + transform->y_pos;
    int16_t max_x = collider->max_x + transform->x_pos;
    int16_t max_y = collider->max_y + transform->y_pos;

    gfx_world_to_screen_coords(&min_x, &min_y);
    gfx_world_to_screen_coords(&max_x, &max_y);

    for (int16_t y = min_y; y <= max_y; y++)
    {
        if (y == min_y || y == max_y)
        {
            for (int16_t x = min_x; x <= max_x; x++)
            {
                if (x < 0 || x >= app->gfx_buffer->width || y < 0 || y >= app->gfx_buffer->height) continue;
                uint16_t buffer_idx = app->gfx_buffer->pixel_size_bytes * (x + (y * app->gfx_buffer->width));

                for (int i = 0; i < app->gfx_buffer->pixel_size_bytes; i++)
                {
                    app->gfx_buffer->pixels[buffer_idx+i] = draw_val;
                }
            }
        }
        else
        {
            if (min_x >= 0 && min_x < app->gfx_buffer->width && y >= 0 && y < app->gfx_buffer->height)
            {
                uint16_t buffer_idx = app->gfx_buffer->pixel_size_bytes * (min_x + (y * app->gfx_buffer->width));

                for (int i = 0; i < app->gfx_buffer->pixel_size_bytes; i++)
                {
                    app->gfx_buffer->pixels[buffer_idx+i] = draw_val;
                }
            }

            if (max_x >= 0 && max_x < app->gfx_buffer->width && y >= 0 && y < app->gfx_buffer->height)
            {
                uint16_t buffer_idx = app->gfx_buffer->pixel_size_bytes * (max_x + (y * app->gfx_buffer->width));

                for (int i = 0; i < app->gfx_buffer->pixel_size_bytes; i++)
                {
                    app->gfx_buffer->pixels[buffer_idx+i] = draw_val;
                }
            }
        }
    }
}

static void gfx_draw_thing(uint32_t thing_idx)
{
    Scene_t *scene = &serializables->scenes[serializables->scene_index];

    if (!scene->things[thing_idx].used) return;

    size_t texture_offset = ephemerals->texture_offsets[thing_get_sprite(&scene->things[thing_idx])->texture_idx];
    Texture_t *texture_ptr = (Texture_t *)(ephemerals->bump_buffer+texture_offset);

    int16_t x = scene->things[thing_idx].transform.x_pos - thing_get_sprite(&scene->things[thing_idx])->x_offset;
    int16_t y = scene->things[thing_idx].transform.y_pos - thing_get_sprite(&scene->things[thing_idx])->y_offset;

    gfx_world_to_screen_coords(&x, &y);

    gfx_draw_texture(texture_ptr, x, y);
}

static void gfx_draw_all_things_debug(void)
{
    static uint16_t step = 0;

    Scene_t *scene = &serializables->scenes[serializables->scene_index];

    Thing_t *step_thing = scene->things+ephemerals->things_draw_order[step];
    /*
    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff),
            "Thing %u [%u,%u] layer %u.", ephemerals->things_draw_order[step],
            step_thing->transform.x_pos, step_thing->transform.y_pos, step_thing->layer);
    platform->debug_log(ephemerals->debug_buff);
    */
    //platform->debug_break();
    step = scene->things_count;

    for (uint16_t i = 0; i < step; i++)
    {
        gfx_draw_thing(ephemerals->things_draw_order[i]);
    }

    for (uint16_t i = 0; i < step; i++)
    {
        gfx_debug_draw_collider(ephemerals->things_draw_order[i], 2);
    }

    /*
    step++;

    if (step >= scene->things_count)
    {
        step = 0;
        debug_gfx = false;
    }
    */
}

static void gfx_draw_all_things(void)
{
    Scene_t *scene = &serializables->scenes[serializables->scene_index];

    for (uint16_t i = 0; i < scene->things_count; i++)
    {
        gfx_draw_thing(ephemerals->things_draw_order[i]);
    }
}

static void load_ephemerals(void)
{
    platform->debug_log("Initializing app ephemeral state.");
    ephemerals->bump_used = 0;

    /// load all textures according to file
    size_t txt_len = 0;

    bzero(ephemerals->scratch, APP_SCRATCH_SIZE);
    txt_len = platform->storage_load_text("textures.soft", (char *)ephemerals->scratch, APP_SCRATCH_SIZE);

    if (txt_len > 0)
    {
        ephemerals->textures_count = 0;

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
        ephemerals->sounds_count = 0;

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
    load_ephemerals();

    /// SERIALIZABLES

    if (serializables->initialized)
    {
        platform->debug_log("Loaded serializable state already initialized.");
    }
    else
    {
        platform->debug_log("Initializing app serializable state.");

        explicit_bzero(serializables, sizeof(*serializables));

        load_scene_by_index(0);

        serializables->controlled_thing_idx = 0;
        serializables->focal_thing_idx = 0;
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

    if(debug_gfx)
    {
        gfx_draw_all_things_debug();
    }
    else
    {
        gfx_draw_all_things();
    }
}

/// must match prototype @ref AppExitFunc
void app_exit(void)
{
}
