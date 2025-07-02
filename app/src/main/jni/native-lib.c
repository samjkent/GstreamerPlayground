#include <android/log.h>
#include <android/native_window_jni.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <jni.h>

#define LOG_TAG "native-lib"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static GstElement *pipeline_capture = NULL;
static GstElement *pipeline_display = NULL;
static GstElement *video_sink = NULL;
static GMainLoop *main_loop = NULL;
static GThread *main_loop_thread = NULL;
static ANativeWindow *native_window = NULL;
static GstElement *appsink = NULL;
static GstElement *appsrc = NULL;
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

GstFlowReturn on_new_sample_from_sink(GstElement *appsink, gpointer user_data) {
  LOGD("on_new_sample_from_sink called");
  static guint64 last_log_time = 0;
  static guint frame_count = 0;

  GstSample *sample;
  GstBuffer *buffer;
  GstFlowReturn ret;
  GstElement *appsrc = GST_ELEMENT(user_data);

  g_signal_emit_by_name(appsink, "pull-sample", &sample);
  if (!sample) {
    LOGE("on_new_sample_from_sink: failed to pull sample");
    return GST_FLOW_ERROR;
  }

  buffer = gst_sample_get_buffer(sample);
  if (!buffer) {
    LOGE("on_new_sample_from_sink: sample has no buffer");
    gst_sample_unref(sample);
    return GST_FLOW_ERROR;
  }

  GstMapInfo map;
  if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
    LOGE("on_new_sample_from_sink: failed to map buffer");
    gst_sample_unref(sample);
    return GST_FLOW_ERROR;
  }

  frame_count++;
  guint64 now = g_get_monotonic_time(); // in microseconds

  if (last_log_time == 0) {
    last_log_time = now;
  }

  double elapsed_sec = (now - last_log_time) / 1e6;
  if (elapsed_sec >= 1.0) {
    double fps = frame_count / elapsed_sec;
    // LOGD("on_new_sample_from_sink: average FPS: %.2f", fps);
    frame_count = 0;
    last_log_time = now;
  }

  // Make a writable copy so we can draw on it
  GstBuffer *outbuf = gst_buffer_copy(buffer);

  // Map output buffer writable
  GstMapInfo outmap;
  gst_buffer_unmap(buffer, &map);

  // Push buffer to appsrc
  g_signal_emit_by_name(appsrc, "push-buffer", outbuf, &ret);

  gst_buffer_unref(outbuf);
  gst_sample_unref(sample);

  return ret;
}

JNIEXPORT void JNICALL
Java_com_example_gstreamerplayground_MainActivity_nativeStartPipeline(
    JNIEnv *env, jobject thiz, jobject surface) {
  // Stop existing pipelines if running
  if (pipeline_capture) {
    LOGD("Stopping existing capture pipeline.");
    gst_element_set_state(pipeline_capture, GST_STATE_NULL);
    gst_object_unref(pipeline_capture);
    pipeline_capture = NULL;
  }

  if (pipeline_display) {
    LOGD("Stopping existing display pipeline.");
    gst_element_set_state(pipeline_display, GST_STATE_NULL);
    gst_object_unref(pipeline_display);
    pipeline_display = NULL;
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

  // ----------- Create CAPTURE pipeline: ahcsrc → videoflip → appsink
  pipeline_capture =
      gst_parse_launch("ahcsrc ! video/x-raw,width=1280,height=720 ! "
                       "videoflip method=clockwise ! videoscale ! videoconvert "
                       "! appsink name=custom_sink",
                       NULL);

  if (!pipeline_capture) {
    LOGE("Failed to create capture pipeline");
    return;
  }

  appsink = gst_bin_get_by_name(GST_BIN(pipeline_capture), "custom_sink");
  if (!appsink) {
    LOGE("Failed to get appsink from capture pipeline");
    gst_object_unref(pipeline_capture);
    pipeline_capture = NULL;
    return;
  }

  GstCaps *sink_caps =
      gst_caps_from_string("video/x-raw,format=RGBA,width=720,height=1280");
  g_object_set(appsink, "caps", sink_caps, "emit-signals", TRUE, "max-buffers",
               1, "drop", TRUE, NULL);
  gst_caps_unref(sink_caps);

  // ----------- Create DISPLAY pipeline: appsrc → queue → videoconvert →
  // glimagesink
  pipeline_display = gst_pipeline_new("display_pipeline");
  if (!pipeline_display) {
    LOGE("Failed to create display pipeline");
    gst_object_unref(pipeline_capture);
    pipeline_capture = NULL;
    return;
  }

  appsrc = gst_element_factory_make("appsrc", "custom_src");
  GstElement *queue = gst_element_factory_make("queue", NULL);
  GstElement *videoconvert = gst_element_factory_make("videoconvert", NULL);
  video_sink = gst_element_factory_make("glimagesink", "videosink");

  if (!appsrc || !queue || !videoconvert || !video_sink) {
    LOGE("Failed to create elements for display pipeline");
    gst_object_unref(pipeline_capture);
    gst_object_unref(pipeline_display);
    pipeline_capture = NULL;
    pipeline_display = NULL;
    return;
  }

  GstCaps *src_caps =
      gst_caps_from_string("video/x-raw,format=RGBA,width=720,height=1280");
  g_object_set(appsrc, "caps", src_caps, "emit-signals", FALSE, "format",
               GST_FORMAT_TIME, "blocksize", 0, "is-live", TRUE, NULL);
  gst_caps_unref(src_caps);

  gst_bin_add_many(GST_BIN(pipeline_display), appsrc, queue, videoconvert,
                   video_sink, NULL);

  if (!gst_element_link_many(appsrc, queue, videoconvert, video_sink, NULL)) {
    LOGE("Failed to link elements in display pipeline");
    gst_object_unref(pipeline_capture);
    gst_object_unref(pipeline_display);
    pipeline_capture = NULL;
    pipeline_display = NULL;
    return;
  }

  // ----------- Connect appsink → appsrc bridge
  g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample_from_sink),
                   appsrc);

  // ----------- Attach glimagesink to native window
  native_window = ANativeWindow_fromSurface(env, surface);
  if (!native_window) {
    LOGE("Failed to get ANativeWindow from surface");
    gst_object_unref(pipeline_capture);
    gst_object_unref(pipeline_display);
    pipeline_capture = NULL;
    pipeline_display = NULL;
    return;
  }
  gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(video_sink),
                                      (guintptr)native_window);

  // ----------- Add bus watches
  GstBus *bus1 = gst_pipeline_get_bus(GST_PIPELINE(pipeline_capture));
  gst_bus_add_watch(bus1, bus_call, NULL);
  gst_object_unref(bus1);

  GstBus *bus2 = gst_pipeline_get_bus(GST_PIPELINE(pipeline_display));
  gst_bus_add_watch(bus2, bus_call, NULL);
  gst_object_unref(bus2);

  // ----------- Start GLib main loop
  main_loop = g_main_loop_new(NULL, FALSE);
  main_loop_thread = g_thread_new("gstreamer_main_loop", main_loop_func, NULL);

  // ----------- Set both pipelines to PLAYING
  if (gst_element_set_state(pipeline_capture, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    LOGE("Failed to set capture pipeline to PLAYING");
    g_main_loop_quit(main_loop);
  } else {
    LOGD("Capture pipeline set to PLAYING");
  }

  if (gst_element_set_state(pipeline_display, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    LOGE("Failed to set display pipeline to PLAYING");
    g_main_loop_quit(main_loop);
  } else {
    LOGD("Display pipeline set to PLAYING");
  }
}

JNIEXPORT void JNICALL
Java_com_example_gstreamerplayground_MainActivity_nativeStopPipeline(
    JNIEnv *env, jobject thiz) {
  LOGD("nativeStopPipeline called");

  if (pipeline_capture) {
    gst_element_set_state(pipeline_capture, GST_STATE_NULL);
    gst_object_unref(pipeline_capture);
    pipeline_capture = NULL;
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
