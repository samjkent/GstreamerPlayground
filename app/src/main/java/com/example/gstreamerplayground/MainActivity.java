package com.example.gstreamerplayground;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import org.freedesktop.gstreamer.GStreamer;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "MainActivity";
    private static final int REQUEST_CAMERA_PERMISSION = 1;

    private SurfaceView surfaceView;
    private TextView statusText;

    static {
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Inflate your XML layout with SurfaceView + TextView
        setContentView(R.layout.activity_main);

        surfaceView = findViewById(R.id.surface_view);
        statusText = findViewById(R.id.status_text);

        try {
            String libPath = getApplicationInfo().nativeLibraryDir + "/" + System.mapLibraryName("gstreamer_android");
            Log.i(TAG, "Expected gstreamer_android.so at: " + libPath);
        } catch (Exception e) {
            e.printStackTrace();
        }

        try {
            GStreamer.init(this);
        } catch (Exception e) {
            e.printStackTrace();
            finish();
        }

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.CAMERA},
                    REQUEST_CAMERA_PERMISSION);
        } else {
            setupSurfaceCallback();
        }
    }

    private void setupSurfaceCallback() {
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                Log.d(TAG, "Surface created");
                statusText.setText("Surface ready. Starting pipeline...");
                startPipeline(holder.getSurface());
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                Log.d(TAG, "Surface changed: " + width + "x" + height);
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                Log.d(TAG, "Surface destroyed");
                statusText.setText("Surface destroyed. Stopping pipeline...");
                stopPipeline();
            }
        });
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode == REQUEST_CAMERA_PERMISSION) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                setupSurfaceCallback();
            } else {
                Log.e(TAG, "Camera permission denied");
                statusText.setText("Camera permission required.");
            }
        }
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }

    private void startPipeline(Surface surface) {
        Log.d(TAG, "Starting pipeline with surface: " + surface);
        nativeStartPipeline(surface);
    }

    private void stopPipeline() {
        Log.d(TAG, "Stopping pipeline");
        nativeStopPipeline();
    }

    private native void nativeStartPipeline(Surface surface);
    private native void nativeStopPipeline();
}

