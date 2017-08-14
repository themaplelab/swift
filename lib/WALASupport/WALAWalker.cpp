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
	  
    printf("FOO: %s\n", text);
	  
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

// Test function for breaking down SILModule SM and exploring integration
void WALAWalker::print(SILModule &SM) {

	// // // // Notes
	// SM.getAssociatedContext() gets the DeclContext
	// SM.getASTContext() gets the ASTContext
	// SM.getSwiftModule() gets the ModuleDecl
	// SM.getSourceManager() gets the SourceManager

	// Debug settings
	bool printToStdout = false;
	bool printSource = false;
	bool printSIL = true;
	bool printPath = true;
	
	// SILModule print() configuration settings
	raw_ostream &outstream = llvm::outs();	// output stream -> stdout
	bool SILLocInfo = true;					// SIL loc info in verbose mode?
	ModuleDecl *module = nullptr;			// types and decls from mod will be printed
	bool sortOutput = false;				// sort functions, witness tables, etc by name?
	bool printASTDecls = true;				// print AST Decls?

	// Output filestamp - uses HHMM-SS as filename
	// TODO: turn into static int counter instead?
// 	time_t now;
// 	struct tm *tPtr;
// 	char timeStr[20];
// 	time(&now);
// 	tPtr = localtime(&now);
// 	strftime(timeStr, 20, "%H%M-%S", tPtr);

	// Get filepath - no error checking here at the moment
// 	char *swiftWalaHome = getenv("SWIFT_WALA_HOME");
// 	char dirPath[1024];
// 	char filePath[1024];
	const char* fileName = SM.getSwiftModule()->getModuleFilename().str().c_str();
// 	sprintf(dirPath, "%s/compileOutput", swiftWalaHome);
// 	sprintf(filePath, "%s/%s.txt", dirPath, timeStr);
// 	printf("-----> Filepath: %s \n\n", filePath);	// DEBUG TEMP

	// Open the file for writing and confirm
// 	ofstream outfile;
// 	outfile.open(filePath, ios::out);
// 	if (outfile.is_open()) {
// 		printf("Successfully opened %s for writing.\n", filePath);
// 	} else {
// 		printf("Error opening %s.  Will not write this file.\n", filePath);	
// 	}

	// Outputs
	if (printSource) {	// Source location information; not currently working
	
// 		if (outfile.is_open()) {
// 			printf("\n\n----- ----- Writing [source] information for %s...\n", fileName);
			// SourceManager information settings
// 			std::cout << "----- ----- Source: \n" << lineAndCol << "\n\n";
// 			auto lineAndCol = SM.getSourceManager().printLineAndColumn(outstream, SM.);
// 		}
	}
	
	if (printSIL) {		// Dump the SIL for the file.  TODO: break this down more atomically
// 		if (outfile.is_open()) {
			printf("\n\n----- ----- Writing [module] information for %s... \n", fileName);
			SM.dump();
// 			SM.dump(filePath);
// 		}
		
		if (printToStdout) SM.print(outstream, SILLocInfo, module, sortOutput, printASTDecls);
	}	

	if (printPath) {	// working with ModuleDecl
// 		if (outfile.is_open()) {
			printf("\n\n----- ----- Writing [path] information for %s...\n\n", fileName);
// 			outfile << " -----> [Path]: " << pathInfo << endl;
// 		}
	}
	
// 	outfile.close();
}