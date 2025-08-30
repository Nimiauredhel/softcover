#ifndef APP_SCENE_H
#define APP_SCENE_H

#define SCENE_ENTITIES_MAX_COUNT (512)

#include "app_entity.h"

typedef struct Scene
{
    bool loaded;
    uint8_t index;
    uint16_t entity_count;
    EntityLive_t entities[SCENE_ENTITIES_MAX_COUNT];
} Scene_t;

#endif
