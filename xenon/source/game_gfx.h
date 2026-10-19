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
