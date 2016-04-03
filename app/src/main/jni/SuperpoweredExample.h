#ifndef Header_SuperpoweredExample
#define Header_SuperpoweredExample

#include <math.h>
#include <pthread.h>

#include "SuperpoweredExample.h"
#include "D:\Work\WorkspaceStudio\Superpowered\SuperpoweredLib/SuperpoweredAdvancedAudioPlayer.h"
#include "D:\Work\WorkspaceStudio\Superpowered\SuperpoweredLib/SuperpoweredFilter.h"
#include "D:\Work\WorkspaceStudio\Superpowered\SuperpoweredLib/SuperpoweredRoll.h"
#include "D:\Work\WorkspaceStudio\Superpowered\SuperpoweredLib/SuperpoweredFlanger.h"

#include "D:\Work\WorkspaceStudio\Superpowered\SuperpoweredLib/SuperpoweredEcho.h"
#include "D:\Work\WorkspaceStudio\Superpowered\SuperpoweredLib/SuperpoweredGate.h"
#include "D:\Work\WorkspaceStudio\Superpowered\SuperpoweredLib/SuperpoweredReverb.h"
#include "D:\Work\WorkspaceStudio\Superpowered\SuperpoweredLib/SuperpoweredWhoosh.h"

#include "D:\Work\WorkspaceStudio\Superpowered\SuperpoweredLib/SuperpoweredAndroidAudioIO.h"

#define NUM_BUFFERS 2
#define HEADROOM_DECIBEL 3.0f
static const float headroom = powf(10.0f, -HEADROOM_DECIBEL * 0.025);

class SuperpoweredExample {
public:

	SuperpoweredExample(const char *path, int *params);
	~SuperpoweredExample();

	bool process(short int *output, unsigned int numberOfSamples);
	void onPlayPause(bool play);
	void onCrossfader(int value);
	void onFxSelect(int value);
	void onFxOff();
	void onFxValue(int value);
	
	void onBackground();
	void onForeground();

	void onOpen(const char *path,int value);
	void onOpen2(const char *path, int *params,int value);

private:
    pthread_mutex_t mutex;
    SuperpoweredAndroidAudioIO *audioSystem;
    SuperpoweredAdvancedAudioPlayer *playerA, *playerB;
    SuperpoweredRoll *roll;
    SuperpoweredFilter *filter;
	SuperpoweredFilter *filter2;
    SuperpoweredFlanger *flanger;
	SuperpoweredEcho *echo;
	SuperpoweredGate *gate;
	SuperpoweredReverb *reverb;
	SuperpoweredWhoosh *whoosh;
    float *stereoBuffer;
    unsigned char activeFx;
    float crossValue, volA, volB;
};

#endif
