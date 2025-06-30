#include <android/log.h>
#include <android/native_window_jni.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <jni.h>

#define LOG_TAG "native-lib"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static GstElement *pipeline = NULL;

JNIEXPORT void JNICALL
Java_com_example_gstreamerplayground_MainActivity_initGStreamer(
    JNIEnv *env, jobject thiz, jobject context, jobject surface) {
  gst_init(NULL, NULL);

  const gchar *pipeline_description =
      "androidcamera ! videoconvert ! autovideosink";

  GError *error = NULL;
  pipeline = gst_parse_launch(pipeline_description, &error);

  if (error) {
    LOGI("GStreamer pipeline error: %s", error->message);
    g_error_free(error);
    return;
  }

  GstElement *sink =
      gst_bin_get_by_interface(GST_BIN(pipeline), GST_TYPE_VIDEO_OVERLAY);
  if (sink && GST_IS_VIDEO_OVERLAY(sink)) {
    ANativeWindow *native_window = ANativeWindow_fromSurface(env, surface);
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sink),
                                        (guintptr)native_window);
    gst_object_unref(sink);
  } else {
    LOGI("Could not get video overlay sink!");
  }

  gst_element_set_state(pipeline, GST_STATE_PLAYING);
}
