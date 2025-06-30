package com.example.gstreamerplayground;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import org.freedesktop.gstreamer.GStreamer;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("native-lib"); // Your JNI code
    }

    private static final int REQUEST_CAMERA_PERMISSION = 1001;
    private SurfaceView surfaceView;

    // Native method to start GStreamer pipeline with surface
    public native void initGStreamer(Object context, Object surface);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
          GStreamer.init(this);
        } catch (Exception e) {
          e.printStackTrace();
          // Handle error or show message to the user
        }

        surfaceView = new SurfaceView(this);
        setContentView(surfaceView);

        if (checkSelfPermission(Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
            setupSurfaceCallback();
        } else {
            requestPermissions(new String[]{Manifest.permission.CAMERA}, REQUEST_CAMERA_PERMISSION);
        }
    }

    private void setupSurfaceCallback() {
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder holder) {
                initGStreamer(MainActivity.this, holder.getSurface());
            }

            @Override
            public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {}

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder holder) {}
        });
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        if (requestCode == REQUEST_CAMERA_PERMISSION) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                setupSurfaceCallback();
            } else {
                // Permission denied, you may want to show a message or exit
                finish();
            }
        } else {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }
    }
}
