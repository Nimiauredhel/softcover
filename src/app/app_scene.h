#ifndef APP_SCENE_H
#define APP_SCENE_H

#define SCENE_THINGS_MAX_COUNT (512)

#include "app_thing.h"

typedef struct Scene
{
    bool loaded;
    uint8_t index;
    uint16_t things_count;
    Thing_t things[SCENE_THINGS_MAX_COUNT];
} Scene_t;

#endif
