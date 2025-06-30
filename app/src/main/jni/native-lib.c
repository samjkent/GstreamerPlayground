#include <android/log.h>
#include <android/native_window_jni.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <jni.h>

#define LOG_TAG "native-lib"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static GstElement *pipeline = NULL;
static GstElement *video_sink = NULL;
static GMainLoop *main_loop = NULL;
static GThread *main_loop_thread = NULL;
static ANativeWindow *native_window = NULL;

/**
 * GStreamer bus callback: handles errors and important messages.
 */
static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data) {
  switch (GST_MESSAGE_TYPE(msg)) {
  case GST_MESSAGE_EOS:
    LOGD("End of stream");
    g_main_loop_quit(main_loop);
    break;
  case GST_MESSAGE_ERROR: {
    GError *err;
    gchar *debug;
    gst_message_parse_error(msg, &err, &debug);
    LOGE("GStreamer error: %s (%s)", err->message, debug);
    g_error_free(err);
    g_free(debug);
    g_main_loop_quit(main_loop);
    break;
  }
  case GST_MESSAGE_WARNING: {
    GError *err;
    gchar *debug;
    gst_message_parse_warning(msg, &err, &debug);
    LOGE("GStreamer warning: %s (%s)", err->message, debug);
    g_error_free(err);
    g_free(debug);
    break;
  }
  default:
    break;
  }
  return TRUE;
}

static void *main_loop_func(void *data) {
  LOGD("Starting GLib main loop");
  g_main_loop_run(main_loop);
  LOGD("GLib main loop exited");
  return NULL;
}

JNIEXPORT void JNICALL
Java_com_example_gstreamerplayground_MainActivity_nativeStartPipeline(
    JNIEnv *env, jobject thiz, jobject surface) {
  LOGD("nativeStartPipeline called");

  // Stop existing pipeline if running
  if (pipeline) {
    LOGD("Pipeline already running. Stopping existing pipeline.");
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    pipeline = NULL;
  }

  if (main_loop) {
    g_main_loop_quit(main_loop);
    g_main_loop_unref(main_loop);
    main_loop = NULL;
  }

  if (main_loop_thread) {
    g_thread_join(main_loop_thread);
    main_loop_thread = NULL;
  }

  if (native_window) {
    ANativeWindow_release(native_window);
    native_window = NULL;
  }

  // Create pipeline: ahcamerasrc → videoconvert → glimagesink
  pipeline = gst_parse_launch(
      "ahcsrc ! video/x-raw,width=1280,height=720 ! videoflip method=clockwise "
      "! videoconvert ! glimagesink name=videosink",
      NULL);
  if (!pipeline) {
    LOGE("Failed to create GStreamer pipeline");
    return;
  }

  video_sink = gst_bin_get_by_name(GST_BIN(pipeline), "videosink");
  if (!video_sink) {
    LOGE("Failed to get video sink from pipeline");
    gst_object_unref(pipeline);
    pipeline = NULL;
    return;
  }

  native_window = ANativeWindow_fromSurface(env, surface);
  if (!native_window) {
    LOGE("Failed to get ANativeWindow from surface");
    gst_object_unref(pipeline);
    pipeline = NULL;
    return;
  }

  // Attach GStreamer video output to Android native window
  gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(video_sink),
                                      (guintptr)native_window);

  // Add bus watch to handle errors/warnings/EOS
  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  gst_bus_add_watch(bus, bus_call, NULL);
  gst_object_unref(bus);

  // Start GLib main loop on a background thread
  main_loop = g_main_loop_new(NULL, FALSE);
  main_loop_thread = g_thread_new("gstreamer_main_loop", main_loop_func, NULL);

  // Start pipeline
  GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    LOGE("Failed to set pipeline to PLAYING");
    g_main_loop_quit(main_loop);
  } else {
    LOGD("Pipeline set to PLAYING");
  }
}

JNIEXPORT void JNICALL
Java_com_example_gstreamerplayground_MainActivity_nativeStopPipeline(
    JNIEnv *env, jobject thiz) {
  LOGD("nativeStopPipeline called");

  if (pipeline) {
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    pipeline = NULL;
  }

  if (main_loop) {
    g_main_loop_quit(main_loop);
    g_main_loop_unref(main_loop);
    main_loop = NULL;
  }

  if (main_loop_thread) {
    g_thread_join(main_loop_thread);
    main_loop_thread = NULL;
  }

  if (native_window) {
    ANativeWindow_release(native_window);
    native_window = NULL;
  }

  LOGD("Pipeline stopped and cleaned up");
}
