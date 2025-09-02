#ifndef APP_CONTROL_H
#define APP_CONTROL_H

#include "common_structs.h"

#define APP_CONTROL_COUNT (12)

typedef enum AppControlIndex
{
    APPCTRLIDX_NONE = -1,
    APPCTRLIDX_MOVE_UP = 0,
    APPCTRLIDX_MOVE_DOWN = 1,
    APPCTRLIDX_MOVE_LEFT = 2,
    APPCTRLIDX_MOVE_RIGHT = 3,
    APPCTRLIDX_SWITCH = 4,
    APPCTRLIDX_SAVE = 5,
    APPCTRLIDX_LOAD = 6,
    APPCTRLIDX_QUIT = 7,
    APPCTRLIDX_VOLUME_UP = 8,
    APPCTRLIDX_VOLUME_DOWN = 9,
    APPCTRLIDX_DEBUG_GFX = 10,
    APPCTRLIDX_RELOAD_EPH = 11,
} AppControlIndex_t;

typedef enum AppControlEventType
{
    /// 0b00 = [is up][no change]
    APPCTRLEVT_NONE = 0x00,
    /// 0b01 = [is up][change]
    APPCTRLEVT_UP = 0x01,
    /// 0b10 = [is down][no change]
    APPCTRLEVT_HOLD = 0x02,
    /// 0b11 = [is down][change]
    APPCTRLEVT_DOWN = 0x03,
} AppControlEventType_t;

typedef struct AppControllerState
{
    int32_t latest[APP_CONTROL_COUNT];
    int32_t prev[APP_CONTROL_COUNT];
} AppControllerState_t;

#endif
