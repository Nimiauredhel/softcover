#include "app_scene.h"
#include "app_common.h"
#include "app_memory.h"

void load_scene_by_path(char *path)
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
                    entity_create(serializables->current_scene_index, definition_index, def_is_local, layer_index, x_pos, y_pos);
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

        scene->loaded = true;

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

void load_scene_by_index(uint8_t index)
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
