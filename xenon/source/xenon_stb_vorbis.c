/* xenon_stb_vorbis.c: build wrapper for the vendored stb_vorbis decoder.
 *
 * We keep the pull-data API and the integer (int16) conversion, but drop the
 * stdio helpers (we decode from a memory blob, never a FILE*). The decoder
 * needs malloc/free, memcpy/memset and a few math calls, all of which libXenon's
 * libc provides on the console.
 */
#define STB_VORBIS_NO_STDIO 1

#include "stb_vorbis.c"
