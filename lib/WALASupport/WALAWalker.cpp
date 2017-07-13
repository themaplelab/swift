#include <stdlib.h>
#include <jni.h>
#include <csetjmp>
#include <stdio.h>

#include "swift/WALASupport/WALAWalker.h"

#include "launch.h"
#include "Exceptions.h"
#include "CAstWrapper.h"

using namespace swift;

void print_object(JNIEnv *java_env, jobject object) {
    jclass Object = java_env->FindClass("java/lang/Object");
	  
    jmethodID toString = java_env->GetMethodID(Object, "toString", "()Ljava/lang/String;");

    jstring msg = (jstring) java_env->CallObjectMethod(object, toString);

    jboolean f = false;
    const char *text = java_env->GetStringUTFChars(msg, &f);
	  
    printf("%s\n", text);
	  
    java_env->ReleaseStringUTFChars(msg, text);
}

void WALAWalker::foo() {
  char *walaHome = getenv("WALA_HOME");
  char classpath[1024];
  sprintf(classpath, "%s/com.ibm.wala.util/target/classes:%s/com.ibm.wala.shrike/target/classes:%s/com.ibm.wala.core/target/classes:%s/com.ibm.wala.cast/target/classes:/Users/dolby/WalaWorkspace/com.ibm.wala.cast.swift/bin", walaHome, walaHome, walaHome, walaHome);

  JNIEnv *java_env = launch(classpath);
  TRY(cpp_ex, java_env)

      jclass xlatorCls = java_env->FindClass("com/ibm/wala/cast/swift/SwiftToCAstTranslator");
      THROW_ANY_EXCEPTION(cpp_ex);

      jmethodID xlatorInit = java_env->GetMethodID(xlatorCls, "<init>", "()V");
      THROW_ANY_EXCEPTION(cpp_ex);

      jobject xlator = java_env->NewObject(xlatorCls, xlatorInit);
      THROW_ANY_EXCEPTION(cpp_ex);

      CAstWrapper CAst(java_env, cpp_ex, xlator);
      THROW_ANY_EXCEPTION(cpp_ex);

      jobject x = CAst.makeConstant(3.7);

      print_object(java_env, x);
      THROW_ANY_EXCEPTION(cpp_ex);
      
  START_CATCH_BLOCK()

      if (java_env->ExceptionCheck()) {
	  jthrowable real_ex = java_env->ExceptionOccurred();
	  
	  print_object(java_env, real_ex);
      }
  
  END_CATCH_BLOCK()
}
