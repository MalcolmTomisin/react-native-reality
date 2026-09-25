#include <jni.h>
#include <fbjni/fbjni.h>
#include "arcoreOnLoad.hpp"
#include "ARApplicationWrapper.h"
#include "ARCoreAvailability.h"

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  facebook::jni::initialize(vm, [] {
    ARApplicationWrapper::registerNatives();
    arcore::registerARCoreInitializationNatives();
  });
  return margelo::nitro::arcore::initialize(vm);
}
