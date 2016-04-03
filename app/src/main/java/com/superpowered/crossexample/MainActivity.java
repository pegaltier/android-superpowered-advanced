package com.superpowered.crossexample;

import android.content.Intent;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.net.Uri;
import android.provider.MediaStore;
import android.support.v4.content.CursorLoader;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.media.AudioManager;
import android.content.Context;
import android.content.res.AssetFileDescriptor;

import java.io.File;
import java.io.IOException;
import android.os.Build;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.Button;
import android.view.View;
import android.widget.Toast;

public class MainActivity extends AppCompatActivity {
    boolean playing = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Get the device's sample rate and buffer size to enable low-latency Android audio output, if available.
        String samplerateString = null, buffersizeString = null;
        if (Build.VERSION.SDK_INT >= 17) {
            AudioManager audioManager = (AudioManager) this.getSystemService(Context.AUDIO_SERVICE);
            samplerateString = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            buffersizeString = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        }
        if (samplerateString == null) samplerateString = "44100";
        if (buffersizeString == null) buffersizeString = "512";

        // Files under res/raw are not compressed, just copied into the APK. Get the offset and length to know where our files are located.
        AssetFileDescriptor fd0 = getResources().openRawResourceFd(R.raw.lycka), fd1 = getResources().openRawResourceFd(R.raw.nuyorica);
        long[] params = {
                fd0.getStartOffset(),
                fd0.getLength(),
                fd1.getStartOffset(),
                fd1.getLength(),
                Integer.parseInt(samplerateString),
                Integer.parseInt(buffersizeString)
        };
        try {
            fd0.getParcelFileDescriptor().close();
            fd1.getParcelFileDescriptor().close();
        } catch (IOException e) {
            android.util.Log.d("", "Close error.");
        }

        // Arguments: path to the APK file, offset and length of the two resource files, sample rate, audio buffer size.
        SuperpoweredExample(getPackageResourcePath(), params);

        // crossfader events
        final SeekBar crossfader = (SeekBar)findViewById(R.id.crossFader);
        crossfader.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {

            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                onCrossfader(progress);
            }

            public void onStartTrackingTouch(SeekBar seekBar) {}
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        // fx fader events
        final SeekBar fxfader = (SeekBar)findViewById(R.id.fxFader);
        fxfader.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {

            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                onFxValue(progress);
            }

            public void onStartTrackingTouch(SeekBar seekBar) {
                onFxValue(seekBar.getProgress());
            }

            public void onStopTrackingTouch(SeekBar seekBar) {
                onFxOff();
            }
        });

        // fx select event
        final RadioGroup group = (RadioGroup)findViewById(R.id.radioGroup1);
        group.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            public void onCheckedChanged(RadioGroup radioGroup, int checkedId) {
                RadioButton checkedRadioButton = (RadioButton)radioGroup.findViewById(checkedId);
                onFxSelect(radioGroup.indexOfChild(checkedRadioButton));
                Toast.makeText(getApplicationContext(),"FX select : "
                        + Integer.toString(radioGroup.indexOfChild(checkedRadioButton)), Toast.LENGTH_LONG).show();
            }
        });
    }

    public void SuperpoweredExample_PlayPause(View button) {  // Play/pause.
        playing = !playing;
        onPlayPause(playing);
        Button b = (Button) findViewById(R.id.playPause);
        b.setText(playing ? "Pause" : "Play");
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onPause() {
        super.onPause();
        onBackground();
    }

    @Override
    protected void onResume() {
        super.onResume();
        onForeground();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        cleanup();
    }

    public void Player1_OpenA(View button) {
        Intent galleryIntent = new Intent(Intent.ACTION_PICK,
                android.provider.MediaStore.Audio.Media.EXTERNAL_CONTENT_URI);
        startActivityForResult(galleryIntent, 1);
    }
    public void Player1_OpenB(View button) {
        Intent galleryIntent = new Intent(Intent.ACTION_PICK,
                android.provider.MediaStore.Audio.Media.EXTERNAL_CONTENT_URI);
        startActivityForResult(galleryIntent, 2);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {

        if (requestCode == 1 || requestCode == 2 || requestCode == 3
                || requestCode == 4) {

            if (resultCode == RESULT_OK) {

                String path = getRealPathFromURI(getApplicationContext(),
                        data.getData());
                int index = path.lastIndexOf("/");
                String fileName = path.substring(index + 1);

                File myFile = new File(path);
                if (myFile.exists()) {
                    Toast.makeText(getApplicationContext(),
                            path + "\n file imported exist", Toast.LENGTH_LONG)
                            .show();
                    onOpen(path, requestCode);
                } else {
                    Toast.makeText(getApplicationContext(),
                            path + "\n file imported not exist",
                            Toast.LENGTH_LONG).show();
                }

            }
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    private String getRealPathFromURI(Context context, Uri contentUri) {
        String[] proj = { MediaStore.Audio.Media.DATA };
        CursorLoader loader = new CursorLoader(context, contentUri, proj, null,
                null, null);
        Cursor cursor = loader.loadInBackground();
        int column_index = cursor
                .getColumnIndexOrThrow(MediaStore.Audio.Media.DATA);
        cursor.moveToFirst();
        return cursor.getString(column_index);
    }


    private native void SuperpoweredExample(String apkPath, long[] offsetAndLength);
    private native void onPlayPause(boolean play);
    private native void onCrossfader(int value);
    private native void onFxSelect(int value);
    private native void onFxOff();
    private native void onFxValue(int value);


    private native void test(String appDir, AssetManager assetManager);
    private native void onBackground();
    private native void onForeground();
    private native void cleanup();
    private native void loadAsset(AssetManager assetManager);
    private native void loadMp3Asset(AssetManager assetManager);
    private native void resample();
    private native void setApkPath(String apkPath);

    public native void onOpen(String path, int value);
    public native void onOpen2(String path, long[] param, int value);

    static {
        System.loadLibrary("SuperpoweredExample");
    }
}
