#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>                                                                                                                                                                                                                        
#include <time.h>                                                                                                                                                                                                                         
#include <sys/stat.h>                                                                                                                                                                                                                     

#include "softcover_platform.h"

#ifndef LIB_NAME
#define LIB_NAME ""
#endif

#define MOCK_TEXTURE_MAX_COUNT (32)
#define MOCK_SPRITE_MAX_COUNT (32)

static const char *app_init_name = "app_init";
static const char *app_loop_name = "app_loop";

static char lib_path[256] = LIB_NAME;

static time_t lib_load_time = {0};
static time_t lib_modified_time = {0};

static void *lib_handle = NULL;
static void (*func_handle)(void) = NULL;

static AppInitFunc app_init;
static AppLoopFunc app_loop;

typedef struct MockTexture
{
    char name[32];
} MockTexture_t;

typedef struct MockSprite
{
    int texture_idx;
    int x;
    int y;
} MockSprite_t;

static uint8_t mock_texture_count = 0;
static uint8_t mock_sprite_count = 0;

static MockTexture_t mock_texture_storage[MOCK_TEXTURE_MAX_COUNT] = {0};
static MockSprite_t mock_sprite_buffer[MOCK_SPRITE_MAX_COUNT] = {0};

void gfx_clear_buffer(void)
{
    mock_sprite_count = 0;
    printf("gfx clear buffer.\n");
}

int gfx_load_texture(char *name)
{
    if (mock_texture_count >= MOCK_TEXTURE_MAX_COUNT) return -1;

    strncpy(mock_texture_storage[mock_texture_count].name, name, 32);
    printf("gfx load texture: %s.\n", name);
    mock_texture_count++;
    return mock_texture_count-1;
}

void gfx_draw_texture(int idx, int x, int y)
{
    if (mock_sprite_count >=MOCK_SPRITE_MAX_COUNT) return;

    printf("gfx draw texture: idx %d, x %d, y %d.\n", idx, x, y);
    mock_sprite_buffer[mock_sprite_count].texture_idx = idx;
    mock_sprite_buffer[mock_sprite_count].x = x;
    mock_sprite_buffer[mock_sprite_count].y = y;
    mock_sprite_count++;
}

volatile char input_read(void)
{
    printf("input read:\n");
    char c = getchar();
    char n = getchar();
    fflush(stdin);
    return c;
}

static void gfx_sync_buffer(void)
{
    printf("== MOCK BUFFER SYNC:\n");

    for (int i = 0; i < mock_sprite_count; i++)
    {
        printf("  %s at [%d, %d]\n",
                mock_texture_storage[mock_sprite_buffer[i].texture_idx].name,
                mock_sprite_buffer[i].x, mock_sprite_buffer[i].y);
    }

    printf("   END MOCK BUFFER SYNC ==\n");
}

static void load_dynamic(void)
{
    printf("Loading dynamic library.\n");

    if (lib_handle != NULL)
    {
        printf("Closing previously loaded handle.\n");
        dlclose(lib_handle);
        lib_handle = NULL;
        func_handle = NULL;
    }

    lib_handle = dlopen(lib_path, RTLD_NOW | RTLD_GLOBAL);

    if (lib_handle == NULL )
    {
        printf("Failed to load [%s]: %s\n", lib_path, dlerror());
        exit(EXIT_FAILURE);
    }

    app_init = dlsym(lib_handle, app_init_name);
    app_loop = dlsym(lib_handle, app_loop_name);

    struct stat lib_stat = {0};
    stat(lib_path, &lib_stat);
    lib_load_time = lib_stat.st_mtime;
}

int main(int argc, char **argv)
{
    static struct stat file_stat = {0};

    printf("Program started.\n");

    if (argc > 1)
    {
        snprintf(lib_path, sizeof(lib_path), "%s", argv[1]);
        printf("Using given library path: %s.\n", lib_path);
    }
    else
    {
        printf("Using default library path: %s.\n", lib_path);
    }

    load_dynamic();

    Platform_t platform =
    {
        .gfx_clear_buffer = gfx_clear_buffer,
        .gfx_draw_texture = gfx_draw_texture,
        .gfx_load_texture = gfx_load_texture,
        .input_read = input_read,
    };

    Memory_t *app_state_mem = malloc(4096 + sizeof(Memory_t));

    if (app_init != NULL)
    {
        app_init(&platform, app_state_mem);
    }
    else
    {
    }

    for(;;)
    {
        if (app_loop != NULL)
        {
            app_loop(&platform, app_state_mem);
        }
        else
        {
        }

        gfx_sync_buffer();

        stat(lib_path, &file_stat);
        lib_modified_time = file_stat.st_mtime;

        if (lib_modified_time != lib_load_time)
        {
            printf("Library modification detected.\n");
            load_dynamic();
        }

        sleep(1);
    }

    return EXIT_SUCCESS;
}
