LOCAL_PATH := $(call my-dir)


PKG_CONFIG := pkg-config
PKG_CONFIG_PATH := $(GSTREAMER_ROOT_ANDROID)/arm64/lib/gstreamer-1.0/pkgconfig
export PKG_CONFIG_PATH

ifndef GSTREAMER_ROOT
ifndef GSTREAMER_ROOT_ANDROID
$(error GSTREAMER_ROOT_ANDROID is not defined!)
endif
GSTREAMER_ROOT            := $(GSTREAMER_ROOT_ANDROID)
endif

GSTREAMER_NDK_BUILD_PATH  := $(GSTREAMER_ROOT)/share/gst-android/ndk-build/
GSTREAMER_PLUGINS         := coreelements ogg theora vorbis audioconvert audioresample playback soup opensles
GSTREAMER_EXTRA_DEPS      := gstreamer-1.0 gstreamer-video-1.0

include $(GSTREAMER_NDK_BUILD_PATH)/gstreamer-1.0.mk

include $(CLEAR_VARS)

LOCAL_MODULE    := native-lib
LOCAL_SRC_FILES := native-lib.c
LOCAL_SHARED_LIBRARIES := gstreamer_android
LOCAL_LDLIBS := -landroid

include $(BUILD_SHARED_LIBRARY)