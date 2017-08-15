#include <stdlib.h>
#include <jni.h>
#include <csetjmp>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>

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

	// Debug/mode settings
	bool DEBUG = true;
	bool printToStdout = true;
	bool printPath = true;
	bool getBreakdown = true;
	bool printSIL = false;
	
	// SILModule print() configuration settings
	raw_ostream &outstream = llvm::outs();	// output stream -> stdout
	bool SILLocInfo = false;				// SIL loc info in verbose mode?
	ModuleDecl *module = nullptr;			// types and decls from mod will be printed
	bool sortOutput = false;				// sort functions, witness tables, etc by name?
	bool printASTDecls = false;				// print AST Decls?
	
	// File output settings
	string filePathName = SM.getSwiftModule()->getModuleFilename().str();
	size_t splitPoint = filePathName.find_last_of("/");
	char *fileName = (char *) filePathName.substr(splitPoint + 1).c_str();
	if (DEBUG) printf("\t\t\t [FILENAME]: %s \n\n", fileName);
	char *outputDir = getenv("SWIFT_WALA_OUTPUT");
	char outputFilename[1024];
	sprintf(outputFilename, "%s/%s.txt", outputDir, fileName);
	
	// Open and check the file
	unsigned i = 0;
	ifstream fileCheck;
	if (DEBUG) printf("\t\t\t[FILE PATH]: %s \n\n", outputFilename);
	fileCheck.open(outputFilename);
	while (fileCheck && i < 100) {
		// File existed; differentiate it until it is unique
		fileCheck.close();
		sprintf(outputFilename, "%s/%s_%u.txt", outputDir, fileName, i);
		if (DEBUG) printf("\t\t\t\t[FILE] - trying %s... \n", outputFilename);
		fileCheck.open(outputFilename);
		i++;
	}
	fileCheck.close();
	
	// Filename is differentiated; open it for output now.
	ofstream outfile;
	outfile.open(outputFilename, ios::out);
	if (!outfile.is_open()) {
		printf("Error opening %s; will not file-dump outputs.\n", outputFilename);
	}
	
// // // // // // Outputs
	
	// Output sourcefile path
	if (printPath) {	
		printf("\n\n----- ----- [Source] file: %s...\n\n", outputFilename);
	}

	// Break apart SILModule -> SILFunction -> SILBasicBlock -> SILInstruction
	if (getBreakdown) {	

		// Iterate SILFunctions: [SILModule].begin() to [SILModule].end()
		for (auto func = SM.begin(); func != SM.end(); ++func) {
			
			// Print SILFunction name
			printf("\t --- --- [Function] Name: %s \n", func->Name);
			printf("\n\n");
			
			// Print SILFunction SIL
// 			printf("\t --- --- [Function] SIL: \n");
// 			func->print(outstream, printFuncVerbose);
// 			func->dump();
// 			printf("\n\n");
			
			// Iterate SILBasicBlock: [SILFunction].begin() to [SILFunction].end()
			for (auto bb = func->begin(); bb != func->end(); ++bb) {
			
				// Print SILBasicBlock operand
				printf("\t\t --- --- [Basic Block] Operand: \n");
				bb->printAsOperand(outstream);
				printf("\n\n");
				
				// Print SILBasicBlock
				printf("\t\t --- --- [Basic Block] Output: \n");
// 				bb->print(outstream);
				
				printf("\t\t\t --- --- [Instructions] \n\n");
				
				// Iterate SILInstruction: [SILBasicBlock].begin() to [SILBasicBlock].end()
				for (auto instr = bb->begin(); instr != bb->end(); ++instr) {
// 					SILLocation loc = instr->getLoc();
					printf("\t\t\t --- --- [Instruction] would be here. \n");
				}	// end SILInstruction iter
				
			} 	// end SILBasicBlock iter
			
			printf("\n\n");
			
		} 	// end SILFunction iter

	} 	// end getBreakdown
	
	// Dump the SIL for the file.  TODO: break this down more atomically
	if (printSIL) {
	
		if (printToStdout) {
			SM.print(outstream, SILLocInfo, module, sortOutput, printASTDecls);
		} else {
			outfile << "[SOURCE] file: %s \n\n" << filePathName;
			SM.dump(outputFilename);
		}
	}
	
	// Close out the file
	if (outfile.is_open()) outfile.close();

}


















