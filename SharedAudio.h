/*
 * SharedAudio.h
 *
 * Wrapper for sharing the X-Plane OpenAL context for playing audio alerts
 *
 * Copyright (C) 2018, Christopher Collins
*/
#ifndef _SHAREDAUDIO_H
#define _SHAREDAUDIO_H

#include "SysOpenAL.h"

/** SetupSharedAudio() checks to see if the audio context exists, and enables
 * the rest of the SharedAudio implementation.
 *
 * If it is not called, then all of the SharedAudio methods and classes will 
 * NOP as not to cause problems.
 *
 * Because OpenAL can be quite late, this should not be called until after 
 * X-Plane's startup is complete.
*/
void SetupSharedAudio();

class AudioSample {
protected:
	ALuint		mAlBuffer;
public:
	AudioSample(const std::string &filename);
	virtual ~AudioSample();
}

#endif