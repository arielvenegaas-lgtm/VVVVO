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
