#include "game_gfx.h"

#include <stdint.h>
#include <string.h>

#include <xenos/xenos.h>
#include <console/console.h>

extern "C" {
#include <ppc/cache.h>
}

#include "gfx.h"

// We reuse the dxs360 Xenos scan-out path (xenon/source/gfx.cpp). The game
// renders into a 320x240 ARGB8888 surface; each frame we scale it (integer,
// nearest) into the Gfx shadow, then Gfx_Present() blits + cache-flushes.

static GfxContext *g_gfx = 0;
static int g_logged = 0;

int VvvvoGfx_Init(void) {
    g_gfx = Gfx_Create();
    return Gfx_Ready(g_gfx) ? 1 : 0;
}

int VvvvoGfx_Width(void)  { return g_gfx ? (int)Gfx_Width(g_gfx)  : 0; }
int VvvvoGfx_Height(void) { return g_gfx ? (int)Gfx_Height(g_gfx) : 0; }

void VvvvoGfx_Clear(uint32_t argb) {
    if (g_gfx) Gfx_Clear(g_gfx, argb);
}

void VvvvoGfx_Text(int x, int y, int scale, uint32_t argb, const char *text) {
    if (g_gfx) Gfx_DrawText(g_gfx, x, y, scale, argb, text);
}

void VvvvoGfx_Present(const uint32_t *pixels, int w, int h, int pitch) {
    if (!g_gfx || !pixels || w <= 0 || h <= 0) return;

    int dw = VvvvoGfx_Width();
    int dh = VvvvoGfx_Height();
    if (dw <= 0 || dh <= 0) return;

    // Integer scale that fits, centred (letterboxed) on the scan-out.
    int scale = dw / w;
    int sy = dh / h;
    if (sy < scale) scale = sy;
    if (scale < 1) scale = 1;

    int outw = w * scale;
    int outh = h * scale;
    int ox = (dw - outw) / 2;
    int oy = (dh - outh) / 2;

    uint32_t *dst = Gfx_Lock(g_gfx, 0);
    if (!dst) return;

    // Clear the visible frame region once, then write our scaled image.
    Gfx_FillRect(g_gfx, ox, oy, outw, outh, 0xFF000000u);

    const uint8_t *src = (const uint8_t *)pixels;
    for (int yy = 0; yy < outh; ++yy) {
        int srcy = yy / scale;
        const uint32_t *srow = (const uint32_t *)(src + (size_t)srcy * pitch);
        int dy = oy + yy;
        if (dy < 0 || dy >= dh) continue;
        uint32_t *drow = dst + (size_t)dy * dw;
        for (int xx = 0; xx < outw; ++xx) {
            int srcx = xx / scale;
            int dx = ox + xx;
            if (dx < 0 || dx >= dw) continue;
            // Source (big-endian surface) word: R<<24 | G<<16 | B<<8 | A<<0.
            // Gfx shadow expects 0xAARRGGBB.
            uint32_t c = srow[srcx];
            uint32_t r = (c >> 24) & 0xFFu;
            uint32_t g = (c >> 16) & 0xFFu;
            uint32_t b = (c >> 8)  & 0xFFu;
            uint32_t a = c & 0xFFu;
            if (a == 0u) { drow[dx] = 0xFF000000u; continue; }
            drow[dx] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }

    Gfx_Present(g_gfx);

    if (!g_logged) {
        g_logged = 1;
    }
}
