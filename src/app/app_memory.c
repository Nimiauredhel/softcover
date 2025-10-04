#include "app_memory.h"

UniformRing_t *input_buffer = NULL;
Texture_t *gfx_buffer = NULL;
UniformRing_t *audio_buffer = NULL;

AppEphemeralState_t *ephemerals = NULL;
AppSerializableState_t *serializables = NULL;

size_t load_texture_to_memory(char *name)
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

size_t load_wav_to_memory(char *name)
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

void load_definitions_all(void)
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
                    ephemerals->definitions[current_idx].flags = ENTITY_FLAGS_NONE;
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
                    ephemerals->definitions[current_idx].flags |= ENTITY_FLAGS_TEXTURE;
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
                    ephemerals->definitions[current_idx].flags |= ENTITY_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].min_x = atoi(value);
                }
                else if (strcmp(token, "COLLISION_MIN_Y") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= ENTITY_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].min_y = atoi(value);
                }
                else if (strcmp(token, "COLLISION_MAX_X") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= ENTITY_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].max_x = atoi(value);
                }
                else if (strcmp(token, "COLLISION_MAX_Y") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= ENTITY_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].max_y = atoi(value);
                }
                else if (strcmp(token, "COLLISION_BLOCK") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= ENTITY_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].flags |= COLL_FLAGS_BLOCK;
                }
                else if (strcmp(token, "COLLISION_SOUND") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= ENTITY_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].flags |= COLL_FLAGS_PLAY_SOUND;
                    ephemerals->colliders[current_idx].params[0] = atoi(value);
                }
                else if (strcmp(token, "COLLISION_SET_SCENE") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= ENTITY_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].flags |= COLL_FLAGS_SET_SCENE;
                    ephemerals->colliders[current_idx].params[1] = atoi(value);
                }
                else if (strcmp(token, "COLLISION_SET_POSITION") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= ENTITY_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].flags |= COLL_FLAGS_SET_POSITION;
                    value = strtok(value, " ");
                    ephemerals->colliders[current_idx].params[2] = atoi(value);
                    value = strtok(NULL, " ");
                    ephemerals->colliders[current_idx].params[3] = atoi(value);
                }
                else if (strcmp(token, "COLLISION_CALLBACK") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= ENTITY_FLAGS_COLLISION;
                    ephemerals->colliders[current_idx].flags |= COLL_FLAGS_CALLBACK;
                    ephemerals->colliders[current_idx].params[4] = atoi(value);
                }
                else if (strcmp(token, "MOVE_SOUND") == 0)
                {
                    ephemerals->definitions[current_idx].flags |= ENTITY_FLAGS_SOUND;
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

void load_ephemerals(void)
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

int32_t global_definition_get_idx_by_name(char *name)
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

int32_t local_definition_get_idx_by_name(char *name)
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

int32_t definition_clone_to_local(int32_t src_idx, bool src_is_local, char *clone_name)
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
