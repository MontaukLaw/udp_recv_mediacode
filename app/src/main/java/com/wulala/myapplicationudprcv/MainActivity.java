package com.wulala.myapplicationudprcv;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import android.graphics.SurfaceTexture;
import android.media.MediaCodec;
import android.os.Bundle;
import android.util.Log;
import android.view.TextureView;
import android.view.View;
import android.widget.Button;

import com.wulala.myapplicationudprcv.databinding.ActivityMainBinding;

import java.io.IOException;
import java.util.Locale;

public class MainActivity extends AppCompatActivity implements View.OnClickListener, TextureView.SurfaceTextureListener {

    private static final String TAG = MainActivity.class.getSimpleName();

    TextureView videoView;
    private MediaCodec mediaCodec;

    private SurfaceTexture surfaceTexture;

    private boolean ifMediaDecoderConfigured = false;
    MyDecoder myDecoder;

    private boolean ifStarted = false;
    SyncDecoder syncDecoder;
    DisplayThread displayThread;

    static {
        System.loadLibrary("myapplicationudprcv");
    }

    private ActivityMainBinding binding;
    private Button btn;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        btn = binding.startRevBtn;
        btn.setOnClickListener(this);


        videoView = binding.textureVideoView;
        videoView.setSurfaceTextureListener(this);

        try {
            mediaCodec = MediaCodec.createDecoderByType("video/avc");
        } catch (IOException e) {
            e.printStackTrace();
        }

        myDecoder = new MyDecoder(mediaCodec);
        //  myDecoder.setMediaDecodeCallback();

        // syncDecoder = new SyncDecoder(mediaCodec);
        displayThread = new DisplayThread(mediaCodec, myDecoder.h264FrameProducer);
    }

    Thread thread;

    public static String byte2HexStr(byte b) {
        String charToHex = Integer.toHexString((char) b);
        if (charToHex.length() > 2) {
            charToHex = charToHex.substring(2);
        } else if (charToHex.length() == 1) {
            charToHex = "0" + charToHex;
        }
        return charToHex.toUpperCase(Locale.ROOT);
    }

    public void getUdpPacket(byte[] data) {
        // Log.d(TAG, "got data len:" + data.length + ", data[4]: 0x" + byte2HexStr(data[4]));
        // Log.d(TAG, "got data len:" + data.length);

        if (data.length == 20 && data[4] == 0x67) {
            Log.d(TAG, "got head");
            if (!ifStarted && ifMediaDecoderConfigured) {
                Log.d(TAG, "start decode");
                mediaCodec.start();
                ifStarted = true;
                // myDecoder.renderTH.start();
                // new Thread(syncDecoder).start();
                // displayThread.run();
                thread = new Thread(displayThread);
                thread.start();
            }
        }

        if (ifStarted) {
            myDecoder.h264FrameProducer.addFrameToQueue(data);
        }
    }

    /**
     * A native method that is implemented by the 'myapplicationudprcv' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();


    public native void threadTest();

    @Override
    public void onClick(View v) {

        switch (v.getId()) {
            case R.id.start_rev_btn:

                threadTest();

//                try {
//                    Test.test();
//                } catch (InterruptedException e) {
//                    e.printStackTrace();
//                }
                break;
        }
    }

    @Override
    public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surface, int width, int height) {
        surfaceTexture = surface;
        myDecoder.configFormat(surfaceTexture);
        // mediaCodec.configure(mediaFormat, new Surface(surfaceTexture), null, 0);
        Log.d(TAG, "Surface ready ");

        ifMediaDecoderConfigured = true;
    }

    @Override
    public void onSurfaceTextureSizeChanged(@NonNull SurfaceTexture surface, int width, int height) {

    }

    @Override
    public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surface) {
        return false;
    }

    @Override
    public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surface) {

    }

    private int inputPackets = 0;
    private int outputPackets = 0;

    private boolean foundFirstPFrame = false;

    // 废弃
    private void foundPFrame(int length) {
        if (!foundFirstPFrame) {
            if (length != 19) {
            } else {
                foundFirstPFrame = true;
            }
        }
    }

    // 测试用
    public void printLog() {
        Log.d(TAG, "printLog: ");
    }

}