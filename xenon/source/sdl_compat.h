#pragma once
// The Xbox 360 (Xenon PPC) is big-endian. Defining SDL_BIG_ENDIAN here makes
// the game's existing endian branches (GraphicsResources.cpp, GraphicsUtil.cpp)
// do the right thing on the target.
#define SDL_BIG_ENDIAN 4321
#define SDL_LIL_ENDIAN 1234
#define SDL_BYTEORDER SDL_BIG_ENDIAN
#include "sdlshim.h"
