/* SharedAudio.cpp
 *
 * C++ wrapper to X-Plane's shared OpenAL context
 */

#include "SharedAudio.h"

static bool		saOpenALOK = false;

void 
SetupSharedAudio()
{
	ALCcontext *cctx = alcGetCurrentContext();
	saOpenALOK = (cctx != nullptr);
}

AudioSample::AudioSample(const std::string &filename) :
	mAlBuffer(0)
{
	alGenBuffers(1, &mAlBuffer);
}

AudioSample::~AudioSample()
{
	if (mAlBuffer != 0) {
		alDeleteBuffers(&mAlBuffer);
		mAlBuffer = 0;
	}
}

