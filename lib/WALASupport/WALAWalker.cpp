#include <stdlib.h>
#include <jni.h>
#include <csetjmp>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>

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
  char *swiftWalaHome = getenv("SWIFT_WALA_HOME");
  char classpath[1024];
  sprintf(classpath, "%s/com.ibm.wala.util/target/classes:%s/com.ibm.wala.shrike/target/classes:%s/com.ibm.wala.core/target/classes:%s/com.ibm.wala.cast/target/classes:%s/com.ibm.wala.cast.swift/bin", walaHome, walaHome, walaHome, walaHome, swiftWalaHome);

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
<<<<<<< HEAD
=======

// Test function for breaking down SILModule SM and exploring integration
void WALAWalker::print(SILModule &SM) {

	// // // // Notes
	// SM.getAssociatedContext() gets the DeclContext
	// SM.getASTContext() gets the ASTContext
	// SM.getSwiftModule() gets the ModuleDecl
	// SM.getSourceManager() gets the SourceManager

	// Debug settings
	int mode = 0;
	bool printSource = false;
	bool printSIL = true;
	bool printPath = true;
	
	// Output file configuration
	time_t now;
	struct tm *timeptr;
	char timeStr[20];
	time(&now);
	timeptr = localtime(&now);
	strftime(timeStr, 20, "%H%M-%S", timeptr);

	printf(" -----> %s\n", timeStr);
	
	char *swiftWalaHome = getenv("SWIFT_WALA_HOME");
	char dirPath[1024];
	char filePath[1024];
	const char* fileName = SM.getSwiftModule()->getModuleFilename().str().c_str();
	printf("-----> [FILE] %s \n", fileName);
	sprintf(dirPath, "%s/compileOutput", swiftWalaHome);

// 	sprintf(filePath, "%s/%s", dirPath, fileName);
	sprintf(filePath, "%s/%s", dirPath, timeStr);

// 	printf("-----> dirPath: %s\n", dirPath);
	printf("-----> Filepath: %s \n\n", filePath);
	ofstream outfile;
	outfile.open(filePath, ios::out);

	// SILModule print() configuration settings
	raw_ostream &outstream = llvm::outs();	// output stream -> stdout
	bool SILLocInfo = true;					// SIL loc info in verbose mode?
	ModuleDecl *module = nullptr;			// types and decls from mod will be printed
	bool sortOutput = false;				// sort functions, witness tables, etc by name?
	bool printASTDecls = true;				// print AST Decls?

	if (printSource) {	// Current working: SIL dump, source location information
	
		printf("\n\n----- ----- Writing [source] information for %s...\n", fileName);
		// SourceManager information settings
//		std::cout << "----- ----- Source: \n" << lineAndCol << "\n\n";
//		auto lineAndCol = SM.getSourceManager().printLineAndColumn(outstream, SM.);
	}
	
	if (printSIL) {
		// Debug-print the module to stream
		printf("\n\n----- ----- Writing [module] information for %s... \n", fileName);
		//SM.print(outstream, SILLocInfo, module, sortOutput, printASTDecls);
		SM.dump(filePath);
	}	

	if (printPath) {	// working with ModuleDecl
		const char *pathInfo = SM.getSwiftModule()->getModuleFilename().str().c_str();
		printf("\n\n----- ----- Writing [path] information for %s...\n\n", pathInfo);
		outfile << " -----> [Path]: " << pathInfo << endl;
	}
	
	outfile.close();
}
>>>>>>> Now outputs a file to /Users/gojeffcho/WalaSwift/swift-wala/compileOutput tagged by HHMM-SS with the dump of the filepath and SILModule.dump().
