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

#ifdef __cplusplus
extern "C" {
#endif

/* Brings up the DAC and clears all voices. Safe to call once at startup. */
void XenonAudio_Init(void);

/* Whether the DAC was initialised successfully. */
int  XenonAudio_Ready(void);

/* Registers the N sound-effect file paths (volume-relative, e.g.
 * "/data/sounds/jump.wav"). Loading is deferred to first play. */
void XenonAudio_SetSfxList(const char *const *paths, int count);

/* Starts (or restarts) sound effect `index`. Ignored if out of range or the
 * file is missing/unreadable. */
void XenonAudio_PlaySfx(int index);

/* Mixes and submits queued audio. Call once per game frame. */
void XenonAudio_Pump(void);

/* Stops all active voices. */
void XenonAudio_StopAll(void);

/* Total number of voices that failed to load (diagnostics). */
int  XenonAudio_LoadFailures(void);

#ifdef __cplusplus
}
#endif
