#include <jni.h>
#include "arcoreOnLoad.hpp"

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  return margelo::nitro::arcore::initialize(vm);
}
