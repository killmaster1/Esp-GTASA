#pragma once
#include <cstdint>
#define JNIEXPORT __attribute__((visibility("default")))
using jlong=int64_t;
using jobject=void*;
struct JNIEnv;
struct JavaVM;
