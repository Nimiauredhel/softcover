#ifndef APP_SERIALIZABLE_STATE_H
#define APP_SERIALIZABLE_STATE_H

#include <stdint.h>
#include <stdbool.h>

#include "app_scene.h"

#define APP_STATE_MAX_SCENES (16)

typedef struct AppSerializableState
{
    bool initialized;
    uint16_t mov_speed;
    uint16_t controlled_entity_idx;
    uint16_t focal_entity_idx;
    uint8_t current_scene_index;
    uint8_t scene_count;
    Scene_t scenes[APP_STATE_MAX_SCENES];
} AppSerializableState_t;

extern AppSerializableState_t *serializables;

#endif
