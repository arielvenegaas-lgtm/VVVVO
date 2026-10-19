#include <SDL.h>

#include "SoundSystem.h"
#include "FileSystemUtils.h"
#include "filesystem.h"

// Silent audio stub: everything below is a no-op until the libXenon
// xenon_sound backend (WAV decode + resample) is added. The game's audio
// call sites are all guarded by m_isValid == false so nothing plays.

MusicTrack::MusicTrack(const char* fileName)
{
	m_isValid = false;
}

MusicTrack::MusicTrack(SDL_RWops *rw)
{
	m_isValid = false;
}

SoundTrack::SoundTrack(const char* fileName)
{
}

SoundSystem::SoundSystem()
{
}

void SoundSystem::playMusic(MusicTrack* music)
{
}

bool hasmusic = false;

CSound::CSound()
{
}

void CSound::Create(int t, int p)
{
}

void CSound::CreateMusic(int t, int p)
{
}

void CSound::playsound()
{
}

void CSound::Release()
{
}

void CSound::stop()
{
}
