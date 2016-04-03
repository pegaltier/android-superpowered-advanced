#include "SuperpoweredExample.h"
#include "SuperpoweredSimple.h"
#include "SuperpoweredResampler.h"
#include "SuperpoweredDecoder.h"
#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>

#define BUFFER_SIZE 8192

static short int *sound;
static int numberOfSoundSamples = 0;
static int playedSamples = 0;
static char *apkPath;

// original
static void playerEventCallbackA(void *clientData, SuperpoweredAdvancedAudioPlayerEvent event, void *value) {
    if (event == SuperpoweredAdvancedAudioPlayerEvent_LoadSuccess) {
    	SuperpoweredAdvancedAudioPlayer *playerA = *((SuperpoweredAdvancedAudioPlayer **)clientData);
        playerA->setPosition(0, false, false);
    };
    if (event == SuperpoweredAdvancedAudioPlayerEvent_EOF) {
        SuperpoweredAdvancedAudioPlayer *playerA = *((SuperpoweredAdvancedAudioPlayer **)clientData);
        // playerA->pause();
    }
}

static void playerEventCallbackB(void *clientData, SuperpoweredAdvancedAudioPlayerEvent event, void *value) {
    if (event == SuperpoweredAdvancedAudioPlayerEvent_LoadSuccess) {
    	SuperpoweredAdvancedAudioPlayer *playerB = *((SuperpoweredAdvancedAudioPlayer **)clientData);
        playerB->setPosition(0, false, false);
    };
    if (event == SuperpoweredAdvancedAudioPlayerEvent_EOF) {
        SuperpoweredAdvancedAudioPlayer *playerB = *((SuperpoweredAdvancedAudioPlayer **)clientData);
        // playerB->pause();
    }
}

static bool audioProcessing(void *clientdata, short int *audioIO, int numberOfSamples, int samplerate) {
	return ((SuperpoweredExample *)clientdata)->process(audioIO, numberOfSamples);
}

SuperpoweredExample::SuperpoweredExample(const char *path, int *params) : activeFx(0), crossValue(0.0f), volB(0.0f), volA(1.0f * headroom) {
    pthread_mutex_init(&mutex, NULL); // This will keep our player volumes and playback states in sync.
    unsigned int samplerate = params[4], buffersize = params[5];
    stereoBuffer = (float *)memalign(16, (buffersize + 16) * sizeof(float) * 2);

    playerA = new SuperpoweredAdvancedAudioPlayer(&playerA , playerEventCallbackA, samplerate, 0);
    playerA->open(path, params[0], params[1]);
    playerB = new SuperpoweredAdvancedAudioPlayer(&playerB, playerEventCallbackB, samplerate, 0);
    playerB->open(path, params[2], params[3]);

    playerA->syncMode = playerB->syncMode = SuperpoweredAdvancedAudioPlayerSyncMode_TempoAndBeat;

    roll = new SuperpoweredRoll(samplerate);
    filter = new SuperpoweredFilter(SuperpoweredFilter_Resonant_Lowpass, samplerate);
    filter2 = new SuperpoweredFilter(SuperpoweredFilter_Resonant_Highpass, samplerate);
    flanger = new SuperpoweredFlanger(samplerate);
	echo = new SuperpoweredEcho(samplerate);
	echo->decay = 0.5;
	gate = new SuperpoweredGate(samplerate);
	reverb = new SuperpoweredReverb(samplerate);
	whoosh = new SuperpoweredWhoosh(samplerate);
	
    audioSystem = new SuperpoweredAndroidAudioIO(samplerate, buffersize, false, true, audioProcessing, this, -1, SL_ANDROID_STREAM_MEDIA, buffersize * 2);
}

SuperpoweredExample::~SuperpoweredExample() {
    delete audioSystem;
    delete playerA;
    delete playerB;
    free(stereoBuffer);
    pthread_mutex_destroy(&mutex);
}

void SuperpoweredExample::onBackground() {
    audioSystem->onBackground();
}

void SuperpoweredExample::onForeground() {
    audioSystem->onForeground();
}

void SuperpoweredExample::onOpen(const char *path, int value) {
    pthread_mutex_lock(&mutex);

    if (value==1) {
        playerA->pause();
        playerA->open(path);
    }
    if (value==2) {
        playerB->pause();
        playerB->open(path);
    }

    pthread_mutex_unlock(&mutex);
}


void SuperpoweredExample::onOpen2(const char *path, int* params, int value) {
    pthread_mutex_lock(&mutex);

    if (value==1) {
        playerA->pause();
        playerA->open(path, params[0], params[1]);
    }
    if (value==2) {
        playerB->pause();
        playerB->open(path, params[0], params[1]);
    }

    pthread_mutex_unlock(&mutex);
}

void SuperpoweredExample::onPlayPause(bool play) {
    pthread_mutex_lock(&mutex);
    if (!play) {
        playerA->pause();
        playerB->pause();
    } else {
        bool masterIsA = (crossValue <= 0.5f);
        playerA->play(!masterIsA);
        playerB->play(masterIsA);
    };
    pthread_mutex_unlock(&mutex);
}

void SuperpoweredExample::onCrossfader(int value) {
    pthread_mutex_lock(&mutex);
    crossValue = float(value) * 0.01f;
    if (crossValue < 0.01f) {
        volA = 1.0f * headroom;
        volB = 0.0f;
    } else if (crossValue > 0.99f) {
        volA = 0.0f;
        volB = 1.0f * headroom;
    } else { // constant power curve
        volA = cosf(M_PI_2 * crossValue) * headroom;
        volB = cosf(M_PI_2 * (1.0f - crossValue)) * headroom;
    };
    pthread_mutex_unlock(&mutex);
}

void SuperpoweredExample::onFxSelect(int value) {
	__android_log_print(ANDROID_LOG_VERBOSE, "SuperpoweredExample", "FXSEL %i", value);
	activeFx = value;
}

void SuperpoweredExample::onFxOff() {
    filter->enable(false);
    filter2->enable(false);
    roll->enable(false);
    flanger->enable(false);
    echo->enable(false);
    gate->enable(false);
    reverb->enable(false);
    whoosh->enable(false);
}

#define MINFREQ 60.0f
#define MAXFREQ 20000.0f

static inline float floatToFrequency(float value) {
    if (value > 0.97f) return MAXFREQ;
    if (value < 0.03f) return MINFREQ;
    value = powf(10.0f, (value + ((0.4f - fabsf(value - 0.4f)) * 0.3f)) * log10f(MAXFREQ - MINFREQ)) + MINFREQ;
    return value < MAXFREQ ? value : MAXFREQ;
}

void SuperpoweredExample::onFxValue(int ivalue) {
    float value = float(ivalue) * 0.01f;
    switch (activeFx) {
        case 1:
            onFxOff();
            filter->setResonantParameters(floatToFrequency(1.0f - value), 0.2f);
            filter->enable(true);
            break;
        case 2:
            onFxOff();
            filter2->setResonantParameters(floatToFrequency(1.0f - value), 0.2f);
            filter2->enable(true);
            break;
        case 3:
            onFxOff();
            if (value > 0.8f) roll->beats = 0.0625f;
            else if (value > 0.6f) roll->beats = 0.125f;
            else if (value > 0.4f) roll->beats = 0.25f;
            else if (value > 0.2f) roll->beats = 0.5f;
            else roll->beats = 1.0f;
            roll->enable(true);
            break;
		case 4:
            onFxOff();
            if (value > 0.8f) echo->beats = 0.0625f;
            else if (value > 0.6f) echo->beats = 0.125f;
            else if (value > 0.4f) echo->beats = 0.25f;
            else if (value > 0.2f) echo->beats = 0.5f;
            else echo->beats = 1.0f;
			echo->setMix(value);
			echo->enable(true);
			break;
		case 5:
            onFxOff();
            if (value > 0.8f) gate->beats = 0.0625f;
            else if (value > 0.6f) gate->beats = 0.125f;
            else if (value > 0.4f) gate->beats = 0.25f;
            else if (value > 0.2f) gate->beats = 0.5f;
            else gate->beats = 4.0f;
			gate->wet = value;
			gate->enable(true);
			break;
		case 6:
            onFxOff();
			reverb->setMix(value);
            reverb->setWidth(1.0f);
            reverb->setDamp(1.0f);
            reverb->setRoomSize(1.0f);
			reverb->enable(true);
			break;
		case 7:
            onFxOff();
			whoosh->wet = value;
			whoosh->enable(true);
			break;
        default:
            onFxOff();
            flanger->setWet(value);
            flanger->enable(true);
    };
}

bool SuperpoweredExample::process(short int *output, unsigned int numberOfSamples) {
    pthread_mutex_lock(&mutex);

    bool masterIsA = (crossValue <= 0.5f);
    float masterBpm = masterIsA ? playerA->currentBpm : playerB->currentBpm;
    double msElapsedSinceLastBeatA = playerA->msElapsedSinceLastBeat; // When playerB needs it, playerA has already stepped this value, so save it now.

    bool silence = !playerA->process(stereoBuffer, false, numberOfSamples, volA, masterBpm, playerB->msElapsedSinceLastBeat);
    if (playerB->process(stereoBuffer, !silence, numberOfSamples, volB, masterBpm, msElapsedSinceLastBeatA)) silence = false;

    roll->bpm = flanger->bpm = echo->bpm = gate->bpm = masterBpm; // Syncing fx is one line.

    if (roll->process(silence ? NULL : stereoBuffer, stereoBuffer, numberOfSamples) && silence) silence = false;
    if (!silence) {
        filter->process(stereoBuffer, stereoBuffer, numberOfSamples);
        filter2->process(stereoBuffer, stereoBuffer, numberOfSamples);
        flanger->process(stereoBuffer, stereoBuffer, numberOfSamples);
		echo->process(stereoBuffer, stereoBuffer, numberOfSamples);
		gate->process(stereoBuffer, stereoBuffer, numberOfSamples);
		reverb->process(stereoBuffer, stereoBuffer, numberOfSamples);
		whoosh->process(stereoBuffer, stereoBuffer, numberOfSamples);
    };

    pthread_mutex_unlock(&mutex);

    // The stereoBuffer is ready now, let's put the finished audio into the requested buffers.
    if (!silence) SuperpoweredFloatToShortInt(stereoBuffer, output, numberOfSamples);
    return !silence;
}

extern "C" {
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_SuperpoweredExample(JNIEnv *javaEnvironment, jobject self, jstring apkPath, jlongArray offsetAndLength);

	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onPlayPause(JNIEnv *javaEnvironment, jobject self, jboolean play);
	
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onCrossfader(JNIEnv *javaEnvironment, jobject self, jint value);
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onFxSelect(JNIEnv *javaEnvironment, jobject self, jint value);
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onFxOff(JNIEnv *javaEnvironment, jobject self);
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onFxValue(JNIEnv *javaEnvironment, jobject self, jint value);
	
	
	
    JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_setApkPath(JNIEnv *javaEnvironment, jobject self, jstring javaApkPath);
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_test(JNIEnv *javaEnvironment, jobject self, jstring cacheDir, jobject javaAssetManager);
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onBackground(JNIEnv *javaEnvironment, jobject self);
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onForeground(JNIEnv *javaEnvironment, jobject self);
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_cleanup(JNIEnv *javaEnvironment, jobject self);
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_loadAsset(JNIEnv *javaEnvironment, jobject self, jobject javaAssetManager);
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_loadMp3Asset(JNIEnv *javaEnvironment, jobject self, jobject javaAssetManager);
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_resample(JNIEnv *javaEnvironment, jobject self);

    JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onOpen(JNIEnv *javaEnvironment, jobject self, jstring spath, jint value);
    JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onOpen2(JNIEnv *javaEnvironment, jobject self, jstring spath, jlongArray params, jint value);

}

static SuperpoweredExample *example = NULL;

// Android is not passing more than 2 custom parameters, so we had to pack file offsets and lengths into an array.
JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_SuperpoweredExample(JNIEnv *javaEnvironment, jobject self, jstring apkPath, jlongArray params) {
	// Convert the input jlong array to a regular int array.
    jlong *longParams = javaEnvironment->GetLongArrayElements(params, JNI_FALSE);
    int arr[6];
    for (int n = 0; n < 6; n++) arr[n] = longParams[n];
    javaEnvironment->ReleaseLongArrayElements(params, longParams, JNI_ABORT);

    const char *path = javaEnvironment->GetStringUTFChars(apkPath, JNI_FALSE);
    example = new SuperpoweredExample(path, arr);
    javaEnvironment->ReleaseStringUTFChars(apkPath, path);

}




JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onPlayPause(JNIEnv *javaEnvironment, jobject self, jboolean play) {
	example->onPlayPause(play);
}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onCrossfader(JNIEnv *javaEnvironment, jobject self, jint value) {
	example->onCrossfader(value);
}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onFxSelect(JNIEnv *javaEnvironment, jobject self, jint value) {
	example->onFxSelect(value);
}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onFxOff(JNIEnv *javaEnvironment, jobject self) {
	example->onFxOff();
}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onFxValue(JNIEnv *javaEnvironment, jobject self, jint value) {
	example->onFxValue(value);
}

JNIEXPORT void Java_com_example_SuperpoweredExample_SuperpoweredPlayer1_onOpen(JNIEnv *javaEnvironment, jobject self, jstring spath, jint value);
JNIEXPORT void Java_com_example_SuperpoweredExample_SuperpoweredPlayer1_onOpen2(JNIEnv *javaEnvironment, jobject self, jstring spath, jlongArray params, jint value);
/*
CUSTOMIZED
*/
JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_setApkPath(JNIEnv *javaEnvironment, jobject self, jstring javaApkPath) {
     const char *path = javaEnvironment->GetStringUTFChars(javaApkPath, JNI_FALSE);

     apkPath = strdup(path);
     __android_log_print(ANDROID_LOG_ERROR, "!", "%s %i", apkPath, strlen(apkPath));

     javaEnvironment->ReleaseStringUTFChars(javaApkPath, path);
}


JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_test(JNIEnv *javaEnvironment, jobject self, jstring cacheDir, jobject javaAssetManager) {
    __android_log_print(ANDROID_LOG_ERROR, "SuperpoweredExample", "test");

    const char *cache_dir = javaEnvironment->GetStringUTFChars(cacheDir, JNI_FALSE);
    __android_log_print(ANDROID_LOG_ERROR, "!!!", "%s", cache_dir);
    chdir(cache_dir);
    javaEnvironment->ReleaseStringUTFChars(cacheDir, cache_dir);

    char path[128];
    char buf[128];
    AAssetManager *mgr = AAssetManager_fromJava(javaEnvironment, javaAssetManager);
    AAssetDir* assetDir = AAssetManager_openDir(mgr, "");
    const char* filename = (const char*) NULL;
    while ((filename = AAssetDir_getNextFileName(assetDir)) != NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "SuperpoweredExample", "filename: %s", filename);

        AAsset* asset = AAssetManager_open(mgr, filename, AASSET_MODE_STREAMING);

        off_t start_index, file_size;
        int descriptor = AAsset_openFileDescriptor(asset, &start_index, &file_size);
        __android_log_print(ANDROID_LOG_ERROR, "SuperpoweredExample", "%d %d %d", descriptor, start_index, file_size);

        AAsset_close(asset);

        sprintf(path, "/proc/self/fd/%d", descriptor);
        int res = readlink(path, buf, 128);
        __android_log_print(ANDROID_LOG_ERROR, "SuperpoweredExample", "%d %s", res, buf);

    }
    AAssetDir_close(assetDir);

    /*AAsset* asset = AAssetManager_open(mgr, "60.mp3", AASSET_MODE_STREAMING);
    off_t start_index = 0;
    off_t file_size = AAsset_getLength(asset);
    int descriptor = AAsset_openFileDescriptor(asset, &start_index, &file_size);*/

    /*char path[128];
    sprintf(path, "/proc/self/fd/%d", descriptor);
    char buf[1024];
    int res = readlink(path, buf, 1024);

    __android_log_print(ANDROID_LOG_ERROR, "!!!", "readlink returned %i %s", res, buf);*/

    // try to play it
    // test this on a new Samsung phone

    // AAsset_close(asset);

}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_loadAsset(JNIEnv *javaEnvironment, jobject self, jobject javaAssetManager) {
    AAssetManager *mgr = AAssetManager_fromJava(javaEnvironment, javaAssetManager);
    AAsset* asset = AAssetManager_open(mgr, "60.wav", AASSET_MODE_STREAMING);
    {
        off_t start_index, file_size;
        int descriptor = AAsset_openFileDescriptor(asset, &start_index, &file_size);
        numberOfSoundSamples = (file_size - 44) >> 2;
        sound = (short int *) memalign(16, (numberOfSoundSamples * 2 + 64) * sizeof(short int));

        AAsset_seek(asset, 44, SEEK_SET);

        char buf[BUFFER_SIZE];
        int nb_read = 0;
        int ind = 0;
        while ((nb_read = AAsset_read(asset, buf, BUFFER_SIZE)) > 0) {
            for (int i = 0; i < nb_read; i += 2) {
                sound[ind++] = buf[i] | (buf[i + 1] << 8);
            }
        }
    }
    AAsset_close(asset);
}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_loadMp3Asset(JNIEnv *javaEnvironment, jobject self, jobject javaAssetManager) {
    AAssetManager *mgr = AAssetManager_fromJava(javaEnvironment, javaAssetManager);
    AAsset* asset = AAssetManager_open(mgr, "60.aac", AASSET_MODE_STREAMING);
    off_t start_index, file_size;
    int descriptor = AAsset_openFileDescriptor(asset, &start_index, &file_size);
    AAsset_close(asset);

    SuperpoweredDecoder *decoder = new SuperpoweredDecoder();
    const char *result = decoder->open(apkPath, false, start_index, file_size);

    __android_log_print(ANDROID_LOG_ERROR, "!", "samplerate: %d", decoder->samplerate);
    __android_log_print(ANDROID_LOG_ERROR, "!", "duration: %f", decoder->durationSeconds);

    sound = (short int *) malloc (((int) decoder->durationSamples * 2 + 64) * sizeof(short int)); // TODO should be something predefined

    short int *pcmOutput = (short int *) malloc(decoder->samplesPerFrame * 2 * sizeof(short int) + 16384);

    while (true) {
        unsigned int samplesDecoded = decoder->samplesPerFrame;
        if (decoder->decode(pcmOutput, &samplesDecoded) == SUPERPOWEREDDECODER_ERROR) {
            break;
        }
        if (samplesDecoded < 1) {
            break;
        }
        memcpy(&sound[numberOfSoundSamples * 2], pcmOutput, samplesDecoded * 2 * sizeof(short int));
        numberOfSoundSamples += samplesDecoded;
    }

    __android_log_print(ANDROID_LOG_ERROR, "!", "numberOfSoundSamples = %i", numberOfSoundSamples);

    free(pcmOutput);
    delete decoder;
}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_resample(JNIEnv *javaEnvironment, jobject self) {
    SuperpoweredResampler *resampler = new SuperpoweredResampler();
    resampler->reset();
    resampler->rate = 44100. / 48000.;

    int size = ((int) ceil(numberOfSoundSamples / resampler->rate)) * 2 + 64;

    float *temp = (float *) memalign(16, size * sizeof(float));
    short int *output = (short int *) memalign(16, size * sizeof(short int));

    int numberOfOutputSamples = resampler->process(
        sound,
        temp,
        output,
        numberOfSoundSamples,
        false,
        false
    );

    free(sound);
    sound = output;
    numberOfSoundSamples = numberOfOutputSamples;
    free(temp);

    delete resampler;
}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onBackground(JNIEnv *javaEnvironment, jobject self) {
    example->onBackground();
}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onForeground(JNIEnv *javaEnvironment, jobject self) {
    example->onForeground();
}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_cleanup(JNIEnv *javaEnvironment, jobject self) {
    delete example;
    free(sound);
}



JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onOpen(JNIEnv *javaEnvironment, jobject self, jstring spath, jint value) {
    const char *path = javaEnvironment->GetStringUTFChars(spath, JNI_FALSE);
    example->onOpen(path,value);
}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onOpen2(JNIEnv *javaEnvironment, jobject self, jstring spath, jlongArray params, jint value) {
    // Convert the input jlong array to a regular int array.
    jlong *longParams = javaEnvironment->GetLongArrayElements(params, JNI_FALSE);
    int arr[2];
    for (int n = 0; n < 2; n++) arr[n] = longParams[n];
    javaEnvironment->ReleaseLongArrayElements(params, longParams, JNI_ABORT);

    const char *path = javaEnvironment->GetStringUTFChars(spath, JNI_FALSE);

    example->onOpen2(path,arr,value);
}
