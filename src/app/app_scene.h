#ifndef APP_SCENE_H
#define APP_SCENE_H

#define SCENE_ENTITIES_MAX_COUNT (512)
#define SCENE_ENTITY_DEFS_MAX_COUNT (8)

#include "app_entity.h"

typedef struct Scene
{
    /// has the scene already been loaded and populated in the app state
    bool loaded;

    /// scene local entity storage
    uint16_t entity_count;
    Entity_t entities[SCENE_ENTITIES_MAX_COUNT];

    /// scene local definition storage
    uint16_t definitions_count;
    EntityDefinition_t definitions[SCENE_ENTITY_DEFS_MAX_COUNT];
    Sprite_t sprites[SCENE_ENTITY_DEFS_MAX_COUNT];
    Collider_t colliders[SCENE_ENTITY_DEFS_MAX_COUNT];
    SoundEmitter_t sound_emitters[SCENE_ENTITY_DEFS_MAX_COUNT];
} Scene_t;

void load_scene_by_path(char *path);
void load_scene_by_index(uint8_t index);

#endif
