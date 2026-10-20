/* stb_vorbis.h: just the declarations from the vendored stb_vorbis.c.
 *
 * stb_vorbis.c doubles as its own header: defining STB_VORBIS_HEADER_ONLY
 * before including it emits the prototypes/typedefs without the implementation.
 * The implementation itself is built once in xenon_stb_vorbis.c.
 */
#pragma once
#define STB_VORBIS_HEADER_ONLY 1
#ifndef STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_STDIO 1
#endif
#include "stb_vorbis.c"
