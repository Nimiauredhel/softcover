#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>                                                                                                                                                                                                                        
#include <time.h>                                                                                                                                                                                                                         
#include <sys/stat.h>                                                                                                                                                                                                                     

#include "softcover_platform.h"

#ifndef LIB_NAME
#define LIB_NAME ""
#endif

static const char *app_init_name = "app_init";
static const char *app_loop_name = "app_loop";

static char lib_path[256] = LIB_NAME;

static time_t lib_load_time = {0};
static time_t lib_modified_time = {0};

static void *lib_handle = NULL;
static void (*func_handle)(void) = NULL;

static void (*app_init)(Platform_t *platform, uint8_t *state_mem, size_t state_mem_len);
static void (*app_loop)(Platform_t *platform, uint8_t *state_mem, size_t state_mem_len);


void gfx_clear_buffer(void)
{
    printf("gfx clear buffer.\n");
}

int gfx_load_texture(char *name)
{
    printf("gfx load texture: %s.\n", name);
    return 0;
}

void gfx_draw_texture(int idx, int x, int y)
{
    printf("gfx draw texture: idx %d, x %d, y %d.\n", idx, x, y);
}

char input_read(void)
{
    printf("input read.\n");
    return 0;
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

    uint8_t app_state_mem[4096] = {0};

    if (app_init != NULL)
    {
        app_init(&platform, app_state_mem, sizeof(app_state_mem));
    }
    else
    {
    }

    for(;;)
    {
        if (app_loop != NULL)
        {
            app_loop(&platform, app_state_mem, sizeof(app_state_mem));
        }
        else
        {
        }

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
