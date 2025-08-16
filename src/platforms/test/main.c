#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>                                                                                                                                                                                                                        
#include <time.h>                                                                                                                                                                                                                         
#include <sys/stat.h>                                                                                                                                                                                                                     

static const char *lib_path = "./app.so";
static const char *func_name = "dynamic_function";

static time_t lib_load_time = {0};
static time_t lib_modified_time = {0};

static void *lib_handle = NULL;
static void (*func_handle)(void) = NULL;

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
        printf("Failed to load .so file: %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    func_handle = dlsym(lib_handle, func_name);

    struct stat lib_stat = {0};
    stat(lib_path, &lib_stat);
    lib_load_time = lib_stat.st_mtime;
}

int main(void)
{
    static struct stat file_stat = {0};

    printf("Program started.\n");

    load_dynamic();

    for(;;)
    {
        if (func_handle != NULL)
        {
            func_handle();
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
