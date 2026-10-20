#pragma once
// Bridge from the ported VVVVVV Screen to the libXenon Xenos framebuffer.
#include <stdint.h>

// Bring up xenos/console and allocate the scan-out shadow. Call once at boot.
int  VvvvoGfx_Init(void);

// Width/height of the Xenos scan-out framebuffer.
int  VvvvoGfx_Width(void);
int  VvvvoGfx_Height(void);

// Present a 320x240 ARGB8888 (0xAARRGGBB) source buffer, integer-scaled and
// centred to the scan-out, then flush to the GPU. `pitch` is in bytes.
void VvvvoGfx_Present(const uint32_t *pixels, int w, int h, int pitch);

// Fill the whole scan-out with a solid colour.
void VvvvoGfx_Clear(uint32_t argb);

// Diagnostic text overlay (uses the port's built-in 5x7 font).
void VvvvoGfx_Text(int x, int y, int scale, uint32_t argb, const char *text);

// Startup diagnostics: draw a small numbered "boot: N" marker in the top-left
// of the scan-out so boot progress is visible even before the game renders.
// Cheap enough to call at each startup stage; not intended for per-frame use.
void BootCheckpoint(int n);

// Full-screen boot/loading screen drawn with the built-in 5x7 font (so it works
// before any PNG has loaded). `pct` is 0..100 of genuinely completed work and
// `stage` names the current step. Clears and presents the scan-out.
void VvvvoGfx_LoadScreen(int pct, const char *stage);

// Full-screen readable error (e.g. missing/corrupt assets). Clears and presents
// the scan-out. `detail` may be NULL.
void VvvvoGfx_ErrorScreen(const char *title, const char *detail);
