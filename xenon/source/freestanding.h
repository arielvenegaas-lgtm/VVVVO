#pragma once
// Freestanding header overrides for missing stdlib/time funcs under libXenon.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
#endif

static inline int abs_int(int x){return x<0?-x:x;}
