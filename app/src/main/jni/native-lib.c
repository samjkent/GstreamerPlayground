#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <gst/gst.h>

/*
 * Java Bindings
 */
static jstring gst_native_get_gstreamer_info (JNIEnv* env, jobject thiz) {
    char *version_utf8 = gst_version_string();
    jstring *version_jstring = (*env)->NewStringUTF(env, version_utf8);
    g_free (version_utf8);
    return version_jstring;
}

JNIEXPORT void JNICALL
Java_com_example_gstreamerplayground_MainActivity_initGStreamer(JNIEnv *env, jobject thiz, jobject context) {
    gst_native_get_gstreamer_info(env, context);
}

static JNINativeMethod native_methods[] = {
        { "nativeGetGStreamerInfo", "()Ljava/lang/String;", (void *) gst_native_get_gstreamer_info}
};

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env = NULL;

    if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        return 0;
    }
    // jclass klass = (*env)->FindClass (env, "org/freedesktop/gstreamer/tutorials/tutorial_1/Tutorial1");
    // (*env)->RegisterNatives (env, klass, native_methods, G_N_ELEMENTS(native_methods));

    return JNI_VERSION_1_4;
}
