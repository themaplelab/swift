#include <stdlib.h>
#include <jni.h>
#include <csetjmp>
#include <fstream>

#include "swift/SIL/SILLocation.h"
#include "swift/WALASupport/WALAWalker.h"
#include "swift/WALASupport/SILWalaInstructionVisitor.h"
#include "swift/Demangling/Demangle.h"

#include "launch.h"
#include "Exceptions.h"
#include "CAstWrapper.h"

using namespace swift;
using ModuleInfo = WALAWalker::ModuleInfo;
using FunctionInfo = WALAWalker::FunctionInfo;
using InstrInfo = WALAWalker::InstrInfo;


//
// 	WALAIntegration:
// 		Methods for passing information to and from WALA.
//
void print_object(JNIEnv *java_env, jobject object) {
	jclass Object = java_env->FindClass("java/lang/Object");
  
	jmethodID toString = java_env->GetMethodID(Object, "toString", "()Ljava/lang/String;");

	jstring msg = (jstring) java_env->CallObjectMethod(object, toString);

	jboolean f = false;
	const char *text = java_env->GetStringUTFChars(msg, &f);
  
	printf("FOO: %s\n", text);
  
	java_env->ReleaseStringUTFChars(msg, text);
}

CAstWrapper* WALAIntegration::operator->() { return CAst; }

jobject WALAIntegration::makePosition(int fl, int fc, int ll, int lc) {
	jclass xlatorCls = 
		java_env->FindClass("com/ibm/wala/cast/ir/translator/NativeTranslatorToCAst");
	THROW_ANY_EXCEPTION(cpp_ex);
	
	jmethodID makePos = java_env->GetMethodID(xlatorCls, 
		"makeLocation", "(IIII)Lcom/ibm/wala/cast/tree/CAstSourcePositionMap$Position;");
	THROW_ANY_EXCEPTION(cpp_ex);

	jobject result = java_env->CallObjectMethod(xlator, makePos, fl, fc, ll, lc);
	THROW_ANY_EXCEPTION(cpp_ex);

	return result;
}

jobject WALAIntegration::makeBigDecimal(const char *strData, int strLen) {
	char *safeData = strndup(strData, strLen);
	jobject val = java_env->NewStringUTF(safeData);
	delete safeData;
	jclass bigDecimalCls = java_env->FindClass("java/math/BigDecimal");
	THROW_ANY_EXCEPTION(cpp_ex);
	jmethodID bigDecimalInit = java_env->GetMethodID(bigDecimalCls, 
		"<init>", "(Ljava/lang/String;)V");
	THROW_ANY_EXCEPTION(cpp_ex);
	jobject bigDecimal = java_env->NewObject(bigDecimalCls, bigDecimalInit, val);
	THROW_ANY_EXCEPTION(cpp_ex);
	java_env->DeleteLocalRef(val);
	return bigDecimal;
}

void WALAIntegration::print(jobject obj) {
	print_object(java_env, obj);
	THROW_ANY_EXCEPTION(cpp_ex);
}
	
WALAIntegration::WALAIntegration(JNIEnv *ijava_env , Exceptions &icpp_ex, 
	const char *file_name) : java_env(ijava_env), cpp_ex(icpp_ex) {

	jclass xlatorCls = 
		java_env->FindClass("com/ibm/wala/cast/swift/SwiftToCAstTranslator");
	THROW_ANY_EXCEPTION(cpp_ex);
	
	jmethodID xlatorInit = java_env->GetMethodID(xlatorCls, 
		"<init>", "(Ljava/lang/String;)V");
	THROW_ANY_EXCEPTION(cpp_ex);

	jstring javaFileName = java_env->NewStringUTF(file_name);
	THROW_ANY_EXCEPTION(cpp_ex);

	print(javaFileName);
	
	xlator = java_env->NewObject(xlatorCls, xlatorInit, javaFileName);
	THROW_ANY_EXCEPTION(cpp_ex);
	
	CAst = new CAstWrapper(java_env, cpp_ex, xlator);
	THROW_ANY_EXCEPTION(cpp_ex);
}


//
//	WALAWalker methods
//

// Main WALAWalker implementation.  Iterates over SILModule -> SILFunctions ->
// SILBasicBlocks -> SILInstructions and extracts all the important information, gathers
// it in InstrInfo, and passes it on to perInstruction().  The logic for handling each
// instruction should be placed in perInstruction(), and analyzeSILModule() should be
// used to create the data structures to perInstruction().
void WALAWalker::analyzeSILModule(SILModule &SM) {
	// WALA-Swift Integration
	char *walaHome = getenv("WALA_HOME");
	char *swiftWalaHome = getenv("SWIFT_WALA_HOME");
	char classpath[1024];
	sprintf(classpath, "%s/com.ibm.wala.util/target/classes:%s/com.ibm.wala.shrike/target/classes:%s/com.ibm.wala.core/target/classes:%s/com.ibm.wala.cast/target/classes:%s/com.ibm.wala.cast.swift/bin", walaHome, walaHome, walaHome, walaHome, swiftWalaHome);

	const char *file_name = strdup(SM.getSwiftModule()->getModuleFilename().str().c_str());

	JNIEnv *java_env = launch_jvm(classpath);
	TRY(cpp_ex, java_env)
        WALAIntegration wala(java_env, cpp_ex, file_name);
        jobject x = wala->makeConstant(3.7);
        wala.print(x);
        SILWalaInstructionVisitor Visitor(wala, true);
        Visitor.visitModule(&SM);
	START_CATCH_BLOCK()
		if (java_env->ExceptionCheck()) {
			jthrowable real_ex = java_env->ExceptionOccurred();
			print_object(java_env, real_ex);
		}
	END_CATCH_BLOCK()
}
