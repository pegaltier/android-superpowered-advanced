package com.superpowered.crossexample;

import android.content.res.AssetManager;

/**
 * Created by pegal on 22/04/2016.
 */
public class MySuperpowered {

    MySuperpowered(String path, long[] params) {
        // Arguments: path to the APK file, offset and length of the two resource files, sample rate, audio buffer size.
        SuperpoweredExample(path, params);
    }


    public native void SuperpoweredExample(String apkPath, long[] offsetAndLength);
    public native void onPlayPause(boolean play);
    public native void onCrossfader(int value);
    public native void onFxSelect(int value);
    public native void onFxOff();
    public native void onFxValue(int value);


    public native void test(String appDir, AssetManager assetManager);
    public native void onBackground();
    public native void onForeground();
    public native void cleanup();
    public native void loadAsset(AssetManager assetManager);
    public native void loadMp3Asset(AssetManager assetManager);
    public native void resample();
    public native void setApkPath(String apkPath);

    public native void onOpen(String path, int value);
    public native void onOpen2(String path, long[] param, int value);

    static {
        System.loadLibrary("SuperpoweredExample");
    }
}
