#ifndef OSPOSIX_H
#define OSPOSIX_H
#include "osmac_stuff.h"

/*
** Defines
*/
#define OS_BASE_PORT 43000
#define UNINITIALIZED 0
#define MAX_PRIORITY 255
#ifndef PTHREAD_STACK_MIN
   #define PTHREAD_STACK_MIN 8092
#endif

#endif
