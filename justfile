build:
   docker build -t gstreamerplayground .

bash: build
  docker run --rm -it -v "$PWD":/workspace -v gradle-cache:/root/.gradle -w /workspace gstreamerplayground /bin/bash

assembleDebug: build
  docker run --rm -it -v "$PWD":/workspace -v "$HOME/.gradle":/root/.gradle -w /workspace gstreamerplayground ./gradlew assembleDebug

installDebug: assembleDebug
  adb uninstall com.example.gstreamerplayground
  adb install -r app/build/outputs/apk/debug/app-debug.apk

logcat:
  adb logcat | grep $(adb shell pidof com.example.gstreamerplayground)
