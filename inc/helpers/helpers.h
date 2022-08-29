#ifndef HELPERS_H
#define HELPERS_H

#ifdef DEBUG
#include <stdio.h>
#define pdebug(fmt, args...) fprintf(stderr, fmt "\n", ## args)
#else
#define pdebug(fmt, args...)
#endif

#endif
