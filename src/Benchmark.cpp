#include "Benchmark.h"
#include <jni.h>
extern "C" JNIEXPORT void JNICALL Java_benchmark_Bridge_command(JNIEnv*,jclass,jint value){benchmark::command.store(value);}
extern "C" JNIEXPORT jint JNICALL Java_benchmark_Bridge_status(JNIEnv*,jclass){return benchmark::status.load();}
extern "C" JNIEXPORT jint JNICALL Java_benchmark_Bridge_repetition(JNIEnv*,jclass){return benchmark::repetition.load();}
