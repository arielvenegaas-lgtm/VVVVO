/* sdlblit.h: pure pixel/blit math shared by the SDL shim and host tests.
 *
 * Kept free of SDL types and of any assumption about CPU endianness: all
 * arithmetic is on decoded 0xRRGGBB / 0xAA values. The caller is responsible
 * for reading/writing the packed words (which is where endianness matters and
 * which is exercised by a separate test).
 */
#pragma once
#include <stdint.h>

/* x / 255 for x <= 65535, with exact results at multiples of 255 (255 -> 1,
 * 65025 -> 255) and 0 -> 0. Standard integer approximation, no divide. */
static inline uint32_t sdl_div255(uint32_t x) { return (x + 1 + (x >> 8)) >> 8; }

/* Extract an 8-bit component given a mask+shift (handles big-endian layouts). */
static inline uint32_t sdl_get(uint32_t word, uint32_t mask, uint8_t shift) {
    if (!mask) return 0;
    return (word & mask) >> shift;
}

/* Pack an 8-bit component into a word at the given shift. */
static inline uint32_t sdl_put(uint32_t value, uint32_t mask, uint8_t shift) {
    if (!mask) return 0;
    return (value << shift) & mask;
}

/* Pixel-draw the game expects: force alpha to opaque, keep 0x00RRGGBB. */
static inline uint32_t sdl_opaque_rgb(uint32_t argb8888) {
    return argb8888 | 0xFF000000u;
}

/* Straight-alpha "source over" of one channel. a in 0..255. */
static inline uint32_t sdl_blend_channel(uint32_t src, uint32_t dst, uint32_t a) {
    uint32_t ia = 255u - a;
    return sdl_div255(src * a + dst * ia);
}

/* Pixel-format description free of SDL types, so the blit core can be compiled
 * and tested on the host. */
typedef struct {
    uint32_t Rmask, Gmask, Bmask, Amask;
    uint8_t  Rshift, Gshift, Bshift, Ashift;
} SdlFmt;

/* Convert one packed source word to the destination format. `blend` applies
 * straight-alpha source-over using `alphaMod`; when 0 the source is copied
 * (with its alpha preserved if the destination has an alpha channel). Returns
 * 1 if a pixel was produced, or 0 if the pixel is fully transparent and must
 * be skipped. */
static inline int sdl_blit_pixel(uint32_t c, const SdlFmt *sf, const SdlFmt *df,
                                 int blend, uint32_t alphaMod, uint32_t *out) {
    uint32_t A = sf->Amask ? sdl_get(c, sf->Amask, sf->Ashift) : 255u;
    int same = (sf->Rshift == df->Rshift && sf->Gshift == df->Gshift &&
                sf->Bshift == df->Bshift && sf->Ashift == df->Ashift &&
                sf->Amask == df->Amask);

    if (!blend) {
        uint32_t o = same ? c
            : (sdl_put(sdl_get(c, sf->Rmask, sf->Rshift), df->Rmask, df->Rshift) |
               sdl_put(sdl_get(c, sf->Gmask, sf->Gshift), df->Gmask, df->Gshift) |
               sdl_put(sdl_get(c, sf->Bmask, sf->Bshift), df->Bmask, df->Bshift));
        if (!same && df->Amask) o |= sdl_put(A, df->Amask, df->Ashift);
        *out = o;
        return 1;
    }

    A = sdl_div255(A * alphaMod);
    if (A == 0) return 0;

    uint32_t sr = sdl_get(c, sf->Rmask, sf->Rshift);
    uint32_t sg = sdl_get(c, sf->Gmask, sf->Gshift);
    uint32_t sb = sdl_get(c, sf->Bmask, sf->Bshift);
    *out = (sdl_put(sr, df->Rmask, df->Rshift) |
            sdl_put(sg, df->Gmask, df->Gshift) |
            sdl_put(sb, df->Bmask, df->Bshift));
    return 1;
}

/* Blend one channel of a `blend` (A<255) pixel against an existing destination
 * word `d` and return the packed result. Split out from sdl_blit_pixel above so
 * the source-over path can be unit-tested with a known destination. */
static inline uint32_t sdl_blit_over(uint32_t c, uint32_t d, const SdlFmt *sf, const SdlFmt *df,
                                     uint32_t A) {
    uint32_t r  = sdl_blend_channel(sdl_get(c, sf->Rmask, sf->Rshift), sdl_get(d, df->Rmask, df->Rshift), A);
    uint32_t g  = sdl_blend_channel(sdl_get(c, sf->Gmask, sf->Gshift), sdl_get(d, df->Gmask, df->Gshift), A);
    uint32_t b  = sdl_blend_channel(sdl_get(c, sf->Bmask, sf->Bshift), sdl_get(d, df->Bmask, df->Bshift), A);
    uint32_t o  = (sdl_put(r, df->Rmask, df->Rshift) |
                   sdl_put(g, df->Gmask, df->Gshift) |
                   sdl_put(b, df->Bmask, df->Bshift));
    if (df->Amask) o |= sdl_put(255u, df->Amask, df->Ashift);
    return o;
}
