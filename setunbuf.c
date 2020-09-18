#include <stdio.h>

static void __attribute__((constructor)) setunbuf(void)
{
    setbuf(stdout, NULL);
}
