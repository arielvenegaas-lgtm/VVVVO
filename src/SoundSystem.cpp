#include <SDL.h>

#include "SoundSystem.h"
#include "FileSystemUtils.h"
#include "filesystem.h"

#include "xenon_audio.h"

// Xbox 360 (libXenon) audio backend.
//
// Sound effects are real: CSound::Create(t, p) plays data/sounds/<t>.wav through
// the libXenon DAC (48 kHz S16 stereo) via the software mixer in xenon_audio.cpp.
//
// Music is NOT played: the original VVVVVV soundtrack is Ogg Vorbis and this
// milestone has no Vorbis decoder. MusicTrack::m_isValid stays false so the
// game's music code paths stay dormant (gameplay is unaffected). See
// COMPATIBILITY.md.

// The 28 sound-effect file names, in the same order as XVVVVX's sfxs[] vector
// (index 0 = jump, ...). These are mapped to volume-relative paths at startup.
static const char *kSfxNames[] = {
    "D:\\data\\sounds\\jump.wav",
    "D:\\data\\sounds\\jump2.wav",
    "D:\\data\\sounds\\hurt.wav",
    "D:\\data\\sounds\\souleyeminijingle.wav",
    "D:\\data\\sounds\\coin.wav",
    "D:\\data\\sounds\\save.wav",
    "D:\\data\\sounds\\crumble.wav",
    "D:\\data\\sounds\\vanish.wav",
    "D:\\data\\sounds\\blip.wav",
    "D:\\data\\sounds\\preteleport.wav",
    "D:\\data\\sounds\\teleport.wav",
    "D:\\data\\sounds\\crew1.wav",
    "D:\\data\\sounds\\crew2.wav",
    "D:\\data\\sounds\\crew3.wav",
    "D:\\data\\sounds\\crew4.wav",
    "D:\\data\\sounds\\crew5.wav",
    "D:\\data\\sounds\\crew6.wav",
    "D:\\data\\sounds\\terminal.wav",
    "D:\\data\\sounds\\gamesaved.wav",
    "D:\\data\\sounds\\crashing.wav",
    "D:\\data\\sounds\\blip2.wav",
    "D:\\data\\sounds\\countdown.wav",
    "D:\\data\\sounds\\go.wav",
    "D:\\data\\sounds\\crash.wav",
    "D:\\data\\sounds\\combine.wav",
    "D:\\data\\sounds\\newrecord.wav",
    "D:\\data\\sounds\\trophy.wav",
    "D:\\data\\sounds\\rescue.wav",
};

static const int kSfxCount = (int)(sizeof(kSfxNames) / sizeof(kSfxNames[0]));
static const char *kMappedSfx[64];
static int g_registered = 0;

// Sound-effect table registration touches the filesystem (MapPath resolves
// volume-relative paths and XenonAudio_Preload reads+decodes every WAV), so it
// MUST NOT run from a global constructor: `CSound sfx;` is a file-scope object
// whose constructor runs during static initialization, before main() and
// before FILESYSTEM_init(). Calling this early walked the FAT/USB driver in an
// uninitialized state and stalled startup right after the video-init message.
// main() now calls SoundSystem_Startup() explicitly, after the FS is up.
static void RegisterSfxOnce() {
    if (g_registered) return;
    // MapPath returns a pointer into a small rotating buffer, so copy each
    // result into stable storage before registering the list.
    static char storage[64][256];
    for (int i = 0; i < kSfxCount && i < 64; ++i) {
        const char *m = MapPath(kSfxNames[i]);
        strncpy(storage[i], m, sizeof(storage[i]) - 1);
        storage[i][sizeof(storage[i]) - 1] = '\0';
        kMappedSfx[i] = storage[i];
    }
    XenonAudio_SetSfxList(kMappedSfx, kSfxCount);
    g_registered = 1;
}

// Deferred startup: safe to call only after FILESYSTEM_init(). Brings up the
// DAC, registers the effect names, and decodes them once. All of this is
// deferred out of the constructors because `CSound sfx;` is a file-scope
// object: its constructor runs during static initialization, before main(),
// before the Xenos framebuffer is up and before the FAT/USB filesystem exists.
void SoundSystem_Startup(void) {
    XenonAudio_Init();
    RegisterSfxOnce();
#if defined(XENON)
    XenonAudio_Preload();
#endif
}

MusicTrack::MusicTrack(const char* fileName)
{
    (void)fileName;
    m_isValid = false;
    m_data = 0;
    m_size = 0;
}

MusicTrack::MusicTrack(SDL_RWops *rw)
{
    m_data = 0;
    m_size = 0;
    m_isValid = false;
    if (rw && rw->data && rw->size)
    {
        m_data = rw->data;
        m_size = rw->size;
        m_isValid = true;
    }
}

SoundTrack::SoundTrack(const char* fileName)
{
    (void)fileName;
}

SoundSystem::SoundSystem()
{
    // Intentionally empty: this object may be constructed during static
    // initialization. All hardware/filesystem work is deferred to
    // SoundSystem_Startup() (see below).
}

void SoundSystem::playMusic(MusicTrack* music)
{
    if (!music || !music->m_isValid || !music->m_data || !music->m_size)
    {
        XenonAudio_MusicStop();
        return;
    }
    XenonAudio_MusicPlay(music->m_data, music->m_size, "music");
}

void SoundSystem::stopMusic()
{
    XenonAudio_MusicStop();
}

void SoundSystem::setMusicVolume(int vol)
{
    XenonAudio_MusicSetVolume(vol);
}

void SoundSystem::pauseMusic()
{
    XenonAudio_MusicStop();
}

bool hasmusic = false;

CSound::CSound()
{
    // Intentionally empty: `CSound sfx;` is a file-scope object constructed
    // during static initialization. Do not touch the DAC or the filesystem
    // here; SoundSystem_Startup() does that after boot has progressed.
}

void CSound::Create(int t, int p)
{
    if (p >= 1)
    {
        XenonAudio_PlaySfx(t);
    }
}

void CSound::CreateMusic(int t, int p)
{
    // No music backend yet (see the file header). Intentionally silent.
    (void)t; (void)p;
}

void CSound::playsound()
{
    // Sounds are triggered directly from Create(); nothing to do here.
}

void CSound::Release()
{
}

void CSound::stop()
{
    XenonAudio_StopAll();
}
