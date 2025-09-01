#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "common_interface.h"
#include "common_structs.h"

#include "app_memory.h"
#include "app_entity.h"
#include "app_scene.h"
#include "app_serializable_state.h"
#include "app_ephemeral_state.h"
#include "app_gfx.h"

static const Platform_t *platform = NULL;

static char input_read_from_buffer(UniformRing_t *input_buffer);
static void input_process_all(void);

static size_t load_texture_to_memory(char *name);
static size_t load_wav_to_memory(char *name);

static void load_definitions_all(void);

static void audio_push_samples(float *samples, uint32_t len, uint8_t channels);
static void audio_push_clip(AudioClip_t *clip);

static void entities_initialize_draw_order(void);
static void entities_update_draw_order(void);
static void entities_get_distance(uint16_t first_entity_id, uint16_t second_entity_id, int32_t *x_dist_out, int32_t *y_dist_out);
static uint16_t entity_test_collision(uint16_t entity_id);
static void entity_move(uint16_t entity_idx, int16_t x_delta, int16_t y_delta);
static int32_t entity_create(uint16_t definition_id, bool local_def, uint8_t layer, int32_t x, int32_t y);
static void entity_set_pos(uint32_t entity_id, int32_t x, int32_t y);
static int8_t entities_compare_y(uint16_t first_id, uint16_t second_id);
static int8_t entities_compare_layer(uint16_t first_id, uint16_t second_id);
static void entities_sort_by_comparer(uint32_t start_idx, uint32_t end_idx, int8_t (*comparer)(uint16_t, uint16_t));

static void load_ephemerals(void);

static char input_read_from_buffer(UniformRing_t *input_buffer)
{
    if (input_buffer == NULL) return '~';

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
        input = input_read_from_buffer(input_buffer);

        switch (input)
        {
            case 'w':
                entity_move(serializables->controlled_entity_idx, 0, -serializables->mov_speed);
                break;
            case 'a':
                entity_move(serializables->controlled_entity_idx, -serializables->mov_speed, 0);
                break;
            case 's':
                entity_move(serializables->controlled_entity_idx, 0, serializables->mov_speed);
                break;
            case 'd':
                entity_move(serializables->controlled_entity_idx, serializables->mov_speed, 0);
                break;
            case 'q':
                serializables->controlled_entity_idx = serializables->controlled_entity_idx == 0 ? 1 : 0;
                serializables->focal_entity_idx = serializables->focal_entity_idx == 0 ? 1 : 0;
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
    size_t remaining = sizeof(ephemerals->bump_buffer) - index;
    Texture_t *texture_ptr = (Texture_t *)(ephemerals->bump_buffer+index);

    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff),
            "Loading texture '%s' to scratch memory at offset %ld, address %p.",
            name, index, (void *)texture_ptr);
    platform->debug_log(ephemerals->debug_buff);

    bool success = platform->gfx_load_texture(name, texture_ptr, remaining);

    if (!success) return -1;

    size_t size = sizeof(Texture_t) + (texture_ptr->height * texture_ptr->width * texture_ptr->pixel_size_bytes);
    ephemerals->bump_used += size;

    ephemerals->texture_offsets[ephemerals->textures_count] = index;
    ephemerals->textures_count++;

    return index;
}

static size_t load_wav_to_memory(char *name)
{
    size_t index = ephemerals->bump_used;
    size_t remaining = sizeof(ephemerals->bump_buffer) - index;
    AudioClip_t *clip_ptr = (AudioClip_t *)(ephemerals->bump_buffer+index);

    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Loading wav '%s' to scratch memory at offset %lu.", name, index);
    platform->debug_log(ephemerals->debug_buff);

    bool success = platform->audio_load_wav(name, clip_ptr, remaining);

    if (!success) return -1;

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

static void load_definitions_all(void)
{
    static const char *filename = "definitions.soft";

    bzero(ephemerals->scratch, APP_SCRATCH_SIZE);
    size_t txt_len = platform->storage_load_text(filename, (char *)ephemerals->scratch, APP_SCRATCH_SIZE);

    if (txt_len > 0)
    {
        ephemerals->definitions_count = 0;

        char *current_line = (char *)ephemerals->scratch;
        char *next_line = NULL;

        uint32_t line_num = 0;
        int32_t current_idx = -1;
        char *token = NULL;
        char *value = NULL;

        while (current_line != NULL && ephemerals->textures_count < APP_SOUNDS_MAX_COUNT)
        {
            next_line = strchr(current_line, '\n');
            if (next_line) *next_line = '\0';
            
            if (strlen(current_line) > 0
                && current_line[0] != '#')
            {
                token = strtok(current_line, "=");
                value = strtok(NULL, "=");

                if (strcmp(token, "DEFINITION") == 0)
                {
                    /// base definition idx on definition count
                    current_idx = ephemerals->definitions_count;
                    /// increment definition count
                    ephemerals->definitions_count++;
                    /// reset flags
                    ephemerals->definitions[current_idx].flags = THING_FLAGS_NONE;
                    /// copy definition name
                    strncpy(ephemerals->definitions[current_idx].name, value, sizeof(ephemerals->definitions[current_idx].name)-1);
                }
                else if (current_idx < 0)
                {
                    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff),
                            "Out of sequence entry (%s:%u): [%s].", filename, line_num, current_line); 
                    platform->debug_log(ephemerals->debug_buff);
                }
                else if (strcmp(token, "TEXTURE_ID") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= THING_FLAGS_TEXTURE;
                    ephemerals->sprites[current_idx].texture_idx = atoi(value);
                }
                else if (strcmp(token, "TEXTURE_OFFSET_X") == 0)
                {
                    ephemerals->sprites[current_idx].x_offset = atoi(value);
                }
                else if (strcmp(token, "TEXTURE_OFFSET_Y") == 0)
                {
                    ephemerals->sprites[current_idx].y_offset = atoi(value);
                }
                else if (strcmp(token, "COLLISION_MIN_X") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= THING_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].min_x = atoi(value);
                }
                else if (strcmp(token, "COLLISION_MIN_Y") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= THING_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].min_y = atoi(value);
                }
                else if (strcmp(token, "COLLISION_MAX_X") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= THING_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].max_x = atoi(value);
                }
                else if (strcmp(token, "COLLISION_MAX_Y") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= THING_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].max_y = atoi(value);
                }
                else if (strcmp(token, "COLLISION_BLOCK") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= THING_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].flags |= COLL_FLAGS_BLOCK;
                }
                else if (strcmp(token, "COLLISION_SOUND") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= THING_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].flags |= COLL_FLAGS_PLAY_SOUND;
                    ephemerals->colliders[current_idx].params[0] = atoi(value);
                }
                else if (strcmp(token, "COLLISION_SET_SCENE") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= THING_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].flags |= COLL_FLAGS_SET_SCENE;
                    ephemerals->colliders[current_idx].params[1] = atoi(value);
                }
                else if (strcmp(token, "COLLISION_SET_POSITION") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= THING_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].flags |= COLL_FLAGS_SET_POSITION;
                    value = strtok(value, " ");
                    ephemerals->colliders[current_idx].params[2] = atoi(value);
                    value = strtok(NULL, " ");
                    ephemerals->colliders[current_idx].params[3] = atoi(value);
                }
                else if (strcmp(token, "COLLISION_CALLBACK") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= THING_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].flags |= COLL_FLAGS_CALLBACK;
                    ephemerals->colliders[current_idx].params[4] = atoi(value);
                }
                else if (strcmp(token, "MOVE_SOUND") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= THING_FLAGS_SOUND;
                    ephemerals->sound_emitters[current_idx].move_sfx_idx = atoi(value);
                }
                else
                {
                    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff),
                            "Unknown entry (%s:%u): [%s].", filename, line_num, current_line); 
                    platform->debug_log(ephemerals->debug_buff);
                }
            }

            line_num++;
            current_line = next_line ? next_line + 1 : NULL;
        }
    }
}

static int32_t global_definition_get_idx_by_name(char *name)
{
    for (uint16_t i = 0; i < ephemerals->definitions_count; i++)
    {
        if (strcmp(name, ephemerals->definitions[i].name) == 0)
        {
            return i;
        }
    }

    return -1;
}

static int32_t local_definition_get_idx_by_name(char *name)
{
    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    for (uint16_t i = 0; i < scene->definitions_count; i++)
    {
        if (strcmp(name, scene->definitions[i].name) == 0)
        {
            return i;
        }
    }

    return -1;
}

static int32_t definition_clone_to_local(int32_t src_idx, bool src_is_local, char *clone_name)
{
    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    if (scene->definitions_count >= SCENE_ENTITY_DEFS_MAX_COUNT)
    {
        platform->debug_log("Cannot clone definition to scene local: limit reached.");
        return -1;
    } 
    else if (src_idx < 0 || (!src_is_local && src_idx >= APP_ENTITY_DEFS_MAX_COUNT)
         || (src_is_local && src_idx >= SCENE_ENTITY_DEFS_MAX_COUNT))
    {
        platform->debug_log("Cannot clone definition: invalid source index.");
        return -1;
    }    

    int32_t dst_idx = scene->definitions_count;
    scene->definitions_count++;

    /// ISSUE: this currently needs to be updated every time a new component is added
    memcpy(scene->definitions+dst_idx,
           (src_is_local ? scene->definitions : ephemerals->definitions)+src_idx, sizeof(EntityDefinition_t));
    memcpy(scene->sprites+dst_idx,
           (src_is_local ? scene->sprites : ephemerals->sprites)+src_idx, sizeof(Sprite_t));
    memcpy(scene->colliders+dst_idx,
           (src_is_local ? scene->colliders : ephemerals->colliders)+src_idx, sizeof(Collider_t));
    memcpy(scene->sound_emitters+dst_idx,
           (src_is_local ? scene->sound_emitters : ephemerals->sound_emitters)+src_idx, sizeof(SoundEmitter_t));
    strncpy(scene->definitions[dst_idx].name,
           clone_name, sizeof(scene->definitions[dst_idx].name));

    return dst_idx;
}

static void load_scene_by_path(char *path)
{
    bzero(ephemerals->scratch, APP_SCRATCH_SIZE);
    size_t txt_len = platform->storage_load_text(path, (char *)ephemerals->scratch, APP_SCRATCH_SIZE);

    if (txt_len > 0)
    {
        Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

        char *current_line = (char *)ephemerals->scratch;
        char *next_line = NULL;

        char *token = NULL;
        char *value = NULL;

        uint32_t line_num = 0;
        int32_t definition_index = -1;
        bool def_is_local = false;
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
                token = strtok(current_line, "=");
                value = strtok(NULL, "=");

                if (strcmp(token, "DEFINITION") == 0)
                {
                    def_is_local = false;
                    definition_index = global_definition_get_idx_by_name(value);
                }
                else if (definition_index < 0)
                {
                    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff),
                            "Out of sequence entry (%s:%u): [%s].", path, line_num, current_line); 
                    platform->debug_log(ephemerals->debug_buff);
                }
                else if (strcmp(token, "LAYER") == 0)
                {
                    layer_index = atoi(value);
                }
                else if (strcmp(token, "INSTANCE_AT") == 0)
                {
                    value = strtok(value, " ");
                    x_pos = atoi(value);
                    value = strtok(NULL, " ");
                    y_pos = atoi(value);
                    entity_create(definition_index, def_is_local, layer_index, x_pos, y_pos);
                }
                else if (strcmp(token, "VARIANT") == 0)
                {
                    /// scene file is specifying and naming a local variant of the selected definition.
                    /// if a local definition with the requested name was already created, use it.
                    int32_t src_idx = definition_index;
                    definition_index = local_definition_get_idx_by_name(value);
                    /// else, clone the base definition to a scene local definition.
                    if (definition_index < 0)
                    {
                        definition_index = definition_clone_to_local(src_idx, def_is_local, value);
                        /// TODO: check return value
                    }
                    def_is_local = true;
                }
                else if (strcmp(token, "COLLISION_SET_SCENE") == 0)
                {
                    scene->colliders[definition_index].flags |= COLL_FLAGS_SET_SCENE;
                    scene->colliders[definition_index].params[1] = atoi(value);
                }
                else if (strcmp(token, "COLLISION_SET_POSITION") == 0)
                {
                    scene->colliders[definition_index].flags |= COLL_FLAGS_SET_POSITION;
                    value = strtok(value, " ");
                    scene->colliders[definition_index].params[2] = atoi(value);
                    value = strtok(NULL, " ");
                    scene->colliders[definition_index].params[3] = atoi(value);
                }
                else if (strcmp(token, "COLLISION_CALLBACK") == 0)
                {
                    scene->colliders[definition_index].flags |= COLL_FLAGS_CALLBACK;
                    scene->colliders[definition_index].params[4] = atoi(value);
                }
                else
                {
                    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff),
                            "Unknown entry (%s:%u): [%s].", path, line_num, current_line); 
                    platform->debug_log(ephemerals->debug_buff);
                }
            }

            line_num++;
            current_line = next_line ? next_line + 1 : NULL;
        }

        serializables->scenes[serializables->current_scene_index].loaded = true;

        snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff),
                "Loaded scene [%s], entity count: %u", path, serializables->scenes[serializables->current_scene_index].entity_count);
        platform->debug_log(ephemerals->debug_buff);

        entities_initialize_draw_order();
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
        serializables->current_scene_index = index;
        entities_initialize_draw_order();
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
            serializables->current_scene_index = index;
            load_scene_by_path(path);
        }
    }
}

static void audio_push_samples(float *samples, uint32_t len, uint8_t channels)
{
    if (audio_buffer == NULL) return;

    if (channels == platform->settings->audio_channels)
    {
        ring_push(audio_buffer, samples, len, false);
    }
    else
    {
        for (uint32_t i = 0; i < len; i+= channels)
        {
            for (uint8_t j = 0; j < platform->settings->audio_channels; j++)
            {
                ring_push(audio_buffer, samples+i+j, 1, false);
            }
        }
    }
}

static void audio_push_clip(AudioClip_t *clip)
{
    audio_push_samples(clip->samples, clip->num_samples, clip->num_channels);
}

static int32_t entity_create(uint16_t definition_id, bool local_def, uint8_t layer, int32_t x, int32_t y)
{
    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];
    uint16_t index = scene->entity_count;

    if (index >= SCENE_ENTITIES_MAX_COUNT)
    {
        platform->debug_log("Cannot add new entity, limit reached.");
        return -1;
    }

    serializables->scenes[serializables->current_scene_index].entity_count++;

    Entity_t *entity = &scene->entities[index];
    EntityDefinition_t *definition = local_def ? &scene->definitions[definition_id] : &ephemerals->definitions[definition_id];

    entity->used = true;
    entity->definition_is_local = local_def;
    entity->layer = layer;
    entity->definition_idx = definition_id;

    entity->transform.x_pos = x;
    entity->transform.y_pos = y;

    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Created new %s[%u] at index %u, [%u,%u] layer %u.",
            definition->name, definition_id, index, x, y, layer);
    platform->debug_log(ephemerals->debug_buff);

    return index;
}

static void entity_set_pos(uint32_t entity_id, int32_t x, int32_t y)
{
    serializables->scenes[serializables->current_scene_index].entities[entity_id].transform.x_pos = x;
    serializables->scenes[serializables->current_scene_index].entities[entity_id].transform.y_pos = y;

    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Set Thing %u at position [%d,%d].", entity_id, x, y);
    platform->debug_log(ephemerals->debug_buff);
}

static int8_t entities_compare_y(uint16_t first_id, uint16_t second_id)
{
    return (serializables->scenes[serializables->current_scene_index].entities[first_id].transform.y_pos
            + entity_get_sprite(first_id)->y_offset)
         - (serializables->scenes[serializables->current_scene_index].entities[second_id].transform.y_pos
            + entity_get_sprite(second_id)->y_offset);
}

static int8_t entities_compare_layer(uint16_t first_id, uint16_t second_id)
{
    return serializables->scenes[serializables->current_scene_index].entities[first_id].layer
         - serializables->scenes[serializables->current_scene_index].entities[second_id].layer;
}

static void entities_sort_by_comparer(uint32_t start_idx, uint32_t end_idx, int8_t (*comparer)(uint16_t, uint16_t))
{
    uint16_t temp;
    bool swapped = false;

    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    for (; end_idx > start_idx; end_idx--)
    {
        if (!scene->entities[ephemerals->entities_draw_order[end_idx]].used)
        {
            continue;
        }

        swapped = false;

        for (uint16_t i = start_idx; i < end_idx; i++)
        {
            if (comparer(
                        ephemerals->entities_draw_order[i],
                        ephemerals->entities_draw_order[i+1])
                        > 0)
            {
                swapped = true;
                temp = ephemerals->entities_draw_order[i];
                ephemerals->entities_draw_order[i] = ephemerals->entities_draw_order[i+1];
                ephemerals->entities_draw_order[i+1] = temp;
            }
        }

        if (!swapped) break;
    }
}

static void entities_initialize_draw_order(void)
{
    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    if (scene->entity_count <= 0)
    {
        platform->debug_log("Cannot initialize draw order - entity count is zero!");
        return;
    }

    /// initialize unsorted state
    for (uint16_t i = 0; i < scene->entity_count; i++)
    {
        ephemerals->entities_draw_order[i] = i;
    }

    /// initialize layer offsets to -1, e.g. no entities using that layer
    for (uint8_t i = 0; i < APP_LAYER_COUNT; i++)
    {
        ephemerals->entities_draw_order_layer_offsets[i] = -1;
    }

    entities_sort_by_comparer(0, scene->entity_count-1, entities_compare_layer);

    uint8_t layer = 0;
    Entity_t *entity;

    // default layer 0 offset before the actual work
    entity = scene->entities+ephemerals->entities_draw_order[0];
    layer = entity->layer;
    ephemerals->entities_draw_order_layer_offsets[entity->layer] = 0;

    for (uint16_t i = 1; i < scene->entity_count; i++)
    {
        entity = scene->entities+ephemerals->entities_draw_order[i];

        if (entity->layer > layer)
        {
            layer = entity->layer;
            ephemerals->entities_draw_order_layer_offsets[layer] = i;
        }
    }
}

static void entities_update_draw_order(void)
{
    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    if (scene->entity_count <= 0)
    {
        platform->debug_log("Cannot update draw order - entity count is zero!");
        return;
    }

    int32_t start_idx = 0;
    int32_t end_idx = 0;

    for (int8_t i = 0; i < APP_LAYER_COUNT; i++)
    {
        start_idx = ephemerals->entities_draw_order_layer_offsets[i];

        if (start_idx < 0) continue;

        end_idx = -1;

        for (int8_t j = i+1; j < APP_LAYER_COUNT; j++)
        {
            if (ephemerals->entities_draw_order_layer_offsets[j] > start_idx)
            {
                end_idx = ephemerals->entities_draw_order_layer_offsets[j] - 1;
                break;
            }
        }

        if (end_idx < 0)
        {
            entities_sort_by_comparer(start_idx, scene->entity_count - 1, entities_compare_y);
            break;
        }

        entities_sort_by_comparer(start_idx, end_idx, entities_compare_y);
    }
}

static void entities_get_distance(uint16_t first_entity_id, uint16_t second_entity_id, int32_t *x_dist_out, int32_t *y_dist_out)
{
    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    *x_dist_out = scene->entities[first_entity_id].transform.x_pos - scene->entities[second_entity_id].transform.x_pos;
    *y_dist_out = scene->entities[first_entity_id].transform.y_pos - scene->entities[second_entity_id].transform.y_pos;

    if (*x_dist_out < 0) *x_dist_out *= -1;
    if (*y_dist_out < 0) *y_dist_out *= -1;
}

static uint16_t entity_test_collision(uint16_t entity_id)
{
    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];
    Entity_t *entity = &scene->entities[entity_id];
    EntityDefinition_t *definition = entity->definition_is_local ? &scene->definitions[entity->definition_idx] : &ephemerals->definitions[entity->definition_idx];

    if (!(definition->flags & THING_FLAGS_COLLISION)) return entity_id;

    Transform_t *this_transform = &scene->entities[entity_id].transform;
    Collider_t *this_collider = entity_get_collider(entity_id);

    Transform_t *other_transform = NULL;
    Collider_t *other_collider = NULL;

    int16_t this_left =   this_transform->x_pos + this_collider->min_x;
    int16_t this_right =  this_transform->x_pos + this_collider->max_x;
    int16_t this_top =    this_transform->y_pos + this_collider->min_y;
    int16_t this_bottom = this_transform->y_pos + this_collider->max_y;

    int16_t other_left = 0;
    int16_t other_right = 0;
    int16_t other_top = 0;
    int16_t other_bottom = 0;

    for (uint16_t i = 0; i < scene->entity_count; i++)
    {
        /// avoid testing against self
        if (i == entity_id) continue;
        /// avoid testing against non-collider
        if (!(entity_get_definition(i)->flags & THING_FLAGS_COLLISION)) continue;

        other_transform = &scene->entities[i].transform;
        other_collider = entity_get_collider(i);

        other_left =   other_transform->x_pos + other_collider->min_x;
        other_right =  other_transform->x_pos + other_collider->max_x;
        other_top =    other_transform->y_pos + other_collider->min_y;
        other_bottom = other_transform->y_pos + other_collider->max_y;

        if(this_right >= other_left
        && this_left <= other_right
        && this_top <= other_bottom
        && this_bottom >= other_top)
        {
            snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Thing %d collided with Thing %d.", entity_id, i);
            platform->debug_log(ephemerals->debug_buff);
            return i;
        }
    }

    return entity_id;
}

static void entity_move(uint16_t entity_id, int16_t x_delta, int16_t y_delta)
{
    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    if (!scene->entities[entity_id].used) return;

    scene->entities[entity_id].transform.x_pos += x_delta;
    scene->entities[entity_id].transform.y_pos += y_delta;

    size_t sfx_offset = ephemerals->sound_offsets[entity_get_sounds(entity_id)->move_sfx_idx];
    audio_push_clip((AudioClip_t *)(ephemerals->bump_buffer+sfx_offset));

    uint16_t other_idx = entity_test_collision(entity_id);

    if (other_idx != entity_id)
    {
        /*
    COLL_FLAGS_NONE = 0x00,
    COLL_FLAGS_BLOCK = 0x01,
    COLL_FLAGS_PLAY_SOUND = 0x02,
    COLL_FLAGS_SET_SCENE = 0x04,
    COLL_FLAGS_SET_POSITION = 0x08,
    COLL_FLAGS_CALLBACK = 0x10,
    */
        Collider_t *other_coll = entity_get_collider(other_idx);

        if (other_coll->flags & COLL_FLAGS_BLOCK)
        {
            entity_set_pos(entity_id, scene->entities[entity_id].transform.x_pos - x_delta, scene->entities[entity_id].transform.y_pos - y_delta);
        }

        if (other_coll->flags & COLL_FLAGS_PLAY_SOUND)
        {
            audio_push_clip((AudioClip_t *)(ephemerals->bump_buffer+ephemerals->sound_offsets[other_coll->params[0]]));
        }

        if ((other_coll->flags & COLL_FLAGS_SET_SCENE)
            && entity_id == serializables->controlled_entity_idx)
        {
            load_scene_by_index(other_coll->params[1]);
            /// TODO: this is obviously a bad way to do it and needs to go later
            serializables->controlled_entity_idx = entity_id;
        }

        if (other_coll->flags & COLL_FLAGS_SET_POSITION)
        {
            entity_set_pos(entity_id, other_coll->params[2], other_coll->params[3]);
        }

        if (other_coll->flags & COLL_FLAGS_CALLBACK)
        {
            /// TODO: add collision callback array
        }

        return;
    }

    /*
    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Thing %d moved to [%d,%d]", entity_idx,
            serializables->entities[entity_idx].x_pos, serializables->entities[entity_idx].y_pos);
    platform->debug_log(ephemerals->debug_buff);
    */
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

            current_line = next_line ? next_line + 1 : NULL;
        }
    }

    /// load all definitions according to file
    load_definitions_all();
}

/// must match prototype @ref AppSetupFunc
void app_setup(Platform_t *interface)
{
    static char debug_buff[128] = {0};

    platform = interface;

    platform->settings->gfx_frame_time_target_us = 16666;

    platform->settings->gfx_pixel_size_bytes = 3;
    platform->settings->gfx_buffer_width = 128;
    platform->settings->gfx_buffer_height = 128;

    size_t required_gfx_memory = 
    platform->settings->gfx_pixel_size_bytes *
    platform->settings->gfx_buffer_width *
    platform->settings->gfx_buffer_height;

    platform->settings->audio_channels = 2;
    platform->settings->audio_buffer_capacity = 32768;

    platform->settings->input_buffer_capacity = 128;

    size_t required_state_memory = sizeof(AppSerializableState_t) + sizeof(AppEphemeralState_t);

    size_t required_memory_total = required_state_memory + required_gfx_memory
        + sizeof(UniformRing_t) + (sizeof(float) * platform->settings->audio_buffer_capacity)
        + sizeof(UniformRing_t) + (sizeof(int) * platform->settings->input_buffer_capacity);
    
    bool sufficient = platform->capabilities->app_memory_max_bytes >= required_memory_total;

    snprintf(debug_buff, sizeof(debug_buff), "App requires %lu bytes of memory, Platform offers up to %lu bytes - %s.",
            required_memory_total, platform->capabilities->app_memory_max_bytes, sufficient ? "proceeding" : "aborting");
    platform->debug_log(debug_buff);

    if (sufficient)
    {
        platform->settings->app_memory_serializable_bytes = sizeof(AppSerializableState_t);
        platform->settings->app_memory_ephemeral_bytes = sizeof(AppEphemeralState_t);
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

    input_buffer = memory->input_buffer;
    gfx_buffer = memory->gfx_buffer;
    audio_buffer = memory->audio_buffer;
    ephemerals = (AppEphemeralState_t *)memory->ephemeral->buffer;
    serializables = (AppSerializableState_t *)memory->serializable->buffer;

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

        serializables->controlled_entity_idx = 0;
        serializables->focal_entity_idx = 0;
        serializables->mov_speed = 1;

        serializables->initialized = true;
    }

    entities_initialize_draw_order();
    entities_update_draw_order();
}

/// must match prototype @ref AppLoopFunc
void app_loop(void)
{
    input_process_all();
    entities_update_draw_order();
    gfx_clear_buffer();

    if(debug_gfx)
    {
        gfx_draw_all_entities_debug();
    }
    else
    {
        gfx_draw_all_entities();
    }
}

/// must match prototype @ref AppExitFunc
void app_exit(void)
{
}
