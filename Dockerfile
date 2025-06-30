# Base image with everything you need
FROM ubuntu:22.04

# Install dependencies for Android + GStreamer builds
RUN apt-get update && \
    apt-get install -y wget curl unzip git openjdk-17-jdk build-essential python3 python3-pip pkg-config

# 1️⃣ Install Android Command Line Tools
ENV ANDROID_SDK_ROOT=/opt/android-sdk
RUN mkdir -p ${ANDROID_SDK_ROOT}/cmdline-tools
WORKDIR ${ANDROID_SDK_ROOT}/cmdline-tools

RUN wget https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip -O cmdline-tools.zip && \
    unzip cmdline-tools.zip && \
    rm cmdline-tools.zip && \
    mv cmdline-tools latest

ENV PATH=$PATH:${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin:${ANDROID_SDK_ROOT}/platform-tools

# Accept licenses and install required SDK packages (adjust API levels to your needs)
RUN yes | sdkmanager --licenses && \
    sdkmanager "platform-tools" "platforms;android-31" "build-tools;31.0.0" "ndk;25.2.9519653"  # Use an NDK version matching your project

# 2️⃣ Download and install GStreamer Android prebuilt binaries
ENV GSTREAMER_ROOT_ANDROID=/opt/gstreamer-android
WORKDIR /opt
RUN mkdir ${GSTREAMER_ROOT_ANDROID}
RUN wget https://gstreamer.freedesktop.org/data/pkg/android/1.26.2/gstreamer-1.0-android-universal-1.26.2.tar.xz && \
    tar xf gstreamer-1.0-android-universal-1.26.2.tar.xz -C ${GSTREAMER_ROOT_ANDROID}

# 3️⃣ Add GStreamer binaries to PATH (optional but useful for dev)
ENV PATH=${GSTREAMER_ROOT_ANDROID}/arm64/bin:${PATH}

# 4️⃣ Copy your entire Android project into the image
WORKDIR /workspace
COPY . /workspace
COPY local.properties.docker /workspace/local.properties
