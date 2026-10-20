/* xenon_audio.h: sound-effect backend for the Xbox 360 port.
 *
 * Uses libXenon's xenon_sound (48 kHz, signed 16-bit stereo, little-endian).
 * WAV sound effects are loaded from the game's data/sounds tree through the
 * FAT32 layer and mixed in software with linear-interpolation resampling.
 *
 * Music is intentionally NOT handled here: the original VVVVVV soundtrack is
 * Ogg Vorbis, which would need a Vorbis decoder. See COMPATIBILITY.md.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Brings up the DAC and clears all voices. Safe to call once at startup. */
void XenonAudio_Init(void);

/* Whether the DAC was initialised successfully. */
int  XenonAudio_Ready(void);

/* Registers the N sound-effect file paths (volume-relative, e.g.
 * "/data/sounds/jump.wav"). Loading is deferred unless Preload() is called. */
void XenonAudio_SetSfxList(const char *const *paths, int count);

/* Decodes every registered effect now, so the first use of an effect does not
 * stall the render thread on USB I/O. Returns the number successfully loaded. */
int  XenonAudio_Preload(void);

/* Starts (or restarts) sound effect `index`. Ignored if out of range or the
 * file is missing/unreadable. */
void XenonAudio_PlaySfx(int index);

/* Mixes and submits queued audio. Call once per game frame. */
void XenonAudio_Pump(void);

/* Stops all active voices. */
void XenonAudio_StopAll(void);

/* Total number of voices that failed to load (diagnostics). */
int  XenonAudio_LoadFailures(void);

/* --- host-test hooks (no-op / used only by tests/ on Linux) ------------- */

/* Number of currently active voices. */
int  XenonAudio_ActiveVoices(void);

/* Loads a WAV from memory into slot `index` (bypassing the FAT32 layer) so the
 * mixer can be tested without a filesystem. Returns 1 on success. */
int  XenonAudio_LoadSlotForTest(int index, const void *wav_bytes, unsigned len);

/* Mixes `frames` stereo frames into `out` (2*frames int16) with no device. */
void XenonAudio_MixForTest(int16_t *out, int frames);

#ifdef __cplusplus
}
#endif
