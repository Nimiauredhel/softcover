#include <stdio.h>
#include <stdint.h>

static const uint8_t magic_num = 123;

void dynamic_function(void)
{
    printf("The magic number is %u !\n", magic_num);
}
