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
  if (gst_buffer_map(outbuf, &outmap, GST_MAP_WRITE)) {
    // Draw a simple FPS overlay in the top-left corner in white
    // Assuming RGBA format, draw pixels manually
    const int width = 1280;
    const int height = 720;
    const int stride = width * 4;
    const int box_w = 200, box_h = 40;

    // Draw a white rectangle (simple indicator, not text rendering)
    for (int y = 0; y < box_h; y++) {
      if (y >= height)
        break;
      for (int x = 0; x < box_w; x++) {
        if (x >= width)
          break;
        guint8 *pixel = &outmap.data[y * stride + x * 4];
        pixel[0] = 255; // R
        pixel[1] = 255; // G
        pixel[2] = 255; // B
        pixel[3] = 255; // A
      }
    }
    gst_buffer_unmap(outbuf, &outmap);
  }

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

  // Create pipeline: ahcsrc → videoflip → appsink
  pipeline = gst_parse_launch("ahcsrc ! video/x-raw,width=1280,height=720 ! "
                              "videoflip method=clockwise ! videoscale ! "
                              "videoconvert",
                              NULL);

  //  ! appsink name=custom_sink",

  if (!pipeline) {
    LOGE("Failed to create GStreamer upstream pipeline");
    return;
  }

  // Get appsink element from pipeline
  appsink = gst_bin_get_by_name(GST_BIN(pipeline), "custom_sink");
  if (!appsink) {
    LOGE("Failed to get appsink from pipeline");
    gst_object_unref(pipeline);
    pipeline = NULL;
    return;
  }

  // Create downstream elements manually: appsrc → videoconvert → glimagesink
  appsrc = gst_element_factory_make("appsrc", "custom_src");

  GstCaps *sink_caps =
      gst_caps_from_string("video/x-raw,format=RGBA,width=720,height=1280");
  g_object_set(appsink, "caps", sink_caps, "emit-signals", TRUE, "max-buffers",
               1, "drop", TRUE, NULL);
  GstCaps *src_caps =
      gst_caps_from_string("video/x-raw,format=RGBA,width=720,height=1280");
  g_object_set(appsrc, "caps", src_caps, "emit-signals", TRUE, "format",
               GST_FORMAT_TIME, "do-timestamp", TRUE, NULL);
  gst_caps_unref(src_caps);
  gst_caps_unref(sink_caps);

  GstElement *videoconvert = gst_element_factory_make("videoconvert", NULL);
  video_sink = gst_element_factory_make("glimagesink", "videosink");
  GstElement *queue = gst_element_factory_make("queue", NULL);

  if (!appsrc || !videoconvert || !video_sink) {
    LOGE("Failed to create downstream elements");
    gst_object_unref(pipeline);
    pipeline = NULL;
    return;
  }

  // Add downstream elements to pipeline
  gst_bin_add_many(GST_BIN(pipeline), queue, videoconvert, video_sink, NULL);

  // Link downstream: appsrc → videoconvert → glimagesink
  if (!gst_element_link_many(queue, videoconvert, video_sink, NULL)) {
    LOGE("Failed to link downstream elements");
    gst_object_unref(pipeline);
    pipeline = NULL;
    return;
  }

  // Configure appsink to emit new-sample signals
  // Connect appsink → appsrc bridge
  g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample_from_sink),
                   appsrc);

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
  GstStateChangeReturn ret;
  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
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
