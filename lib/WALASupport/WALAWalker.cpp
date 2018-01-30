#include <stdlib.h>
#include <jni.h>
#include <csetjmp>
#include <fstream>

#include "swift/SIL/SILLocation.h"
#include "swift/WALASupport/WALAWalker.h"
#include "swift/WALASupport/InstrKindInfoGetter.h"
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

// Gets the mangled and demangled SILFunction and returns in a FunctionInfo.
FunctionInfo WALAWalker::getSILFunctionInfo(SILFunction &func) {

	FunctionInfo funcInfo;
	funcInfo.name = func.getName();
	StringRef demangled = Demangle::demangleSymbolAsString(funcInfo.name);
	funcInfo.demangled = demangled;

	return funcInfo;
}

// Gets the sourcefile, start line/col, end line/col, and writes it to the InstrInfo 
// that is passed in.  
// TODO: check lastBuffer vs. buffer to see if start and end are in the same file
void WALAWalker::getInstrSrcInfo(SILInstruction &instr, InstrInfo *instrInfo) {
	
	SourceManager &srcMgr = instr.getModule().getSourceManager();
	
	// Get file-line-col information for the source
	SILLocation debugLoc = instr.getDebugLocation().getLocation();
	SILLocation::DebugLoc debugInfo = debugLoc.decodeDebugLoc(srcMgr);
	
	instrInfo->Filename = debugInfo.Filename;
	
	if (instr.getLoc().isNull()) {
		instrInfo->srcType = -1;
	} else {

		SourceRange srcRange = instr.getLoc().getSourceRange();
		SourceLoc srcStart = srcRange.Start;
		SourceLoc srcEnd = srcRange.End;
		
		if (srcStart.isValid() ) {

			instrInfo->srcType = 0;
			
			auto startLineCol = srcMgr.getLineAndColumn(srcStart);	
			instrInfo->startLine = startLineCol.first;
			instrInfo->startCol = startLineCol.second;			

		} else {
			instrInfo->startLine = debugInfo.Line;
			instrInfo->startCol = debugInfo.Column;
		}
		
		if (srcEnd.isValid() ) {
		
			auto endLineCol = srcMgr.getLineAndColumn(srcEnd);
			instrInfo->endLine = endLineCol.first;
			instrInfo->endCol = endLineCol.second;
		} else {
			instrInfo->srcType = 1;
		}
	}
}

// Gets the ValueKind of the SILInstruction then goes through the mega-switch to handle 
// appropriately.  
// TODO: currently only returns ValueKind, switch is not descended into functionally
SILInstructionKind WALAWalker::getInstrValueKindInfo(SILInstruction &instr, WALAIntegration &wala, 
											unordered_map<void*, jobject>* nodeMap, list<jobject>* nodeList,
											SymbolTable* symbolTable, BasicBlockLabeller* labeller) {

	raw_ostream& outs = llvm::outs();

	outs << "address of instr below me: " << &instr << "\n";

	InstrKindInfoGetter instrKindInfoGetter(&instr, &wala, nodeMap, nodeList, symbolTable, labeller, &outs);

	return instrKindInfoGetter.get();
}

// Main WALAWalker implementation.  Iterates over SILModule -> SILFunctions -> 
// SILBasicBlocks -> SILInstructions and extracts all the important information, gathers
// it in InstrInfo, and passes it on to perInstruction().  The logic for handling each
// instruction should be placed in perInstruction(), and analyzeSILModule() should be
// used to create the data structures to perInstruction().
void WALAWalker::analyzeSILModule(SILModule &SM) {

	// Module Information container
	ModuleInfo modInfo;

	// Entry point file:
	modInfo.sourcefile = SM.getSwiftModule()->getModuleFilename();
	if (modInfo.sourcefile.empty() ) { modInfo.sourcefile = "N/A"; }

	// WALA-Swift Integration
	char *walaHome = getenv("WALA_HOME");
	char *swiftWalaHome = getenv("SWIFT_WALA_HOME");
	char classpath[1024];
	sprintf(classpath, "%s/com.ibm.wala.util/target/classes:%s/com.ibm.wala.shrike/target/classes:%s/com.ibm.wala.core/target/classes:%s/com.ibm.wala.cast/target/classes:%s/com.ibm.wala.cast.swift/bin", walaHome, walaHome, walaHome, walaHome, swiftWalaHome);
	
	const char *file_name = strdup(modInfo.sourcefile.str().c_str());
	
// 	JNIEnv *java_env = launch_jvm(classpath);
  JNIEnv *java_env = launch_jvm(classpath);
	TRY(cpp_ex, java_env)		
		WALAIntegration wala(java_env, cpp_ex, file_name);
		jobject x = wala->makeConstant(3.7);
		wala.print(x);
	
	// Iterate over SILFunctions
	SymbolTable* symbolTable = new SymbolTable();
	for (auto func = SM.begin(); func != SM.end(); ++func) {
		
		FunctionInfo funcInfo = getSILFunctionInfo(*func);
		list<jobject>* blockStmtList = new list<jobject>();
		BasicBlockLabeller* labeller = new BasicBlockLabeller();
		
		llvm::outs() << "SILFunction: " ;
    llvm::outs() << &*func << "\n";
    func->print(llvm::outs(), true);

		// Iterate over SILBasicBlocks
		for (auto bb = func->begin(); bb != func->end(); ++bb) {
			
			llvm::outs() << "Basic Block: " ;
			llvm::outs() << &*bb << "\n";
			llvm::outs() << "SILFunctions: " << bb->getParent() << "\n";
			unsigned i = 0; 	// for Instruction count
			unordered_map<void*, jobject>* nodeMap = new unordered_map<void*, jobject>();
			list<jobject>* nodeList = new list<jobject>();

			// Iterate over SILInstructions
			for (auto instr = bb->begin(); instr != bb->end(); ++instr) {
				
				SILPrintContext Ctx(llvm::outs());
	
				InstrInfo instrInfo;
				getInstrSrcInfo(*instr, &instrInfo);
				
				instrInfo.memBehavior = instr->getMemoryBehavior();
				instrInfo.relBehavior = instr->getReleasingBehavior();

				instrInfo.num = i;
				instrInfo.modInfo = &modInfo;
				instrInfo.funcInfo = &funcInfo;
				instrInfo.instrKind = getInstrValueKindInfo(*instr, wala, nodeMap, nodeList, symbolTable, labeller);

				// Get each operand <SILValue> and save it in a vector; get instr ID
				ArrayRef<Operand> ops = instr->getAllOperands();
				std::vector<SILValue> vals;				
				for (auto op = ops.begin(); op != ops.end(); ++op) {
					instrInfo.id = Ctx.getID(op->get());
					vals.push_back(op->get());
				}
				
				// Store vector as ArrayRef<SILValue> in instrInfo
				instrInfo.ops = llvm::ArrayRef<SILValue>(vals);
				
				// Pass instrInfo to perInstruction, where actions can be taken
				perInstruction(&instrInfo, wala);
				
				++i;
			
			}	// end SILInstruction
			if (nodeList->size() > 0) {
				jobject labelNode = wala->makeConstant(labeller->label(&*bb).c_str());
				jobject labelStmt = wala->makeNode(CAstWrapper::LABEL_STMT, labelNode, nodeList->front());
				nodeList->pop_front();
				nodeList->push_front(labelStmt);
				jobject blockStmt = wala->makeNode(CAstWrapper::BLOCK_STMT, wala->makeArray(nodeList));
				blockStmtList->push_back(blockStmt);
			}
			delete nodeMap;
			delete nodeList;
		}	// end SILBasicBlock
		for (auto blockStmt : *blockStmtList) {
			wala.print(blockStmt);
		}
		delete blockStmtList;
		delete labeller;
	}	// end SILFunction
	delete symbolTable;

	START_CATCH_BLOCK()
		if (java_env->ExceptionCheck()) {
			jthrowable real_ex = java_env->ExceptionOccurred();
			print_object(java_env, real_ex);
		}
	END_CATCH_BLOCK()
}

// Actions to take on a per-instruction basis.  instrInfo contains all the relevant info
// for the current instruction in the iteration.
void WALAWalker::perInstruction(InstrInfo *instrInfo, WALAIntegration &wala) {

	raw_ostream &outs = llvm::outs();
	
	wala.print(wala.makePosition(
		instrInfo->startLine, instrInfo->startCol, 
		instrInfo->endLine, instrInfo->endCol)
	);

	if (this->printStdout) {
		outs << "\t [INSTR] #" << instrInfo->num;
		outs << ", [OPNUM] " << instrInfo->id << "\n";
		outs << "\t --> File: " << instrInfo->Filename << "\n";

		// Has no location information
		if (instrInfo->srcType == -1) {
			outs << "\t **** No source information. \n";
			
		// Has only start information
		} else {
			outs << "\t ++++ Start - Line " << instrInfo->startLine << ":" 
				<< instrInfo->startCol << "\n";
		}

		// Has end information
		if (instrInfo->srcType == 0) {
			outs << "\t ---- End - Line " << instrInfo->endLine;
			outs << ":" << instrInfo->endCol << "\n";
		}
		
		// Memory Behavior
		switch (instrInfo->memBehavior) {
			case SILInstruction::MemoryBehavior::None: {
				break;
			}
			case SILInstruction::MemoryBehavior::MayRead: {
				outs	<< "\t\t +++ [MEM-R]: May read from memory. \n";
				break;
			}
			case SILInstruction::MemoryBehavior::MayWrite: {
				outs 	<< "\t\t +++ [MEM-W]: May write to memory. \n";
				break;
			}
			case SILInstruction::MemoryBehavior::MayReadWrite: {
				outs 	<< "\t\t +++ [MEM-RW]: May read or write memory. \n";
				break;
			}
			case SILInstruction::MemoryBehavior::MayHaveSideEffects: {
				outs 	<< "\t\t +++ [MEM-F]: May have side effects. \n";
			}
		}
		
		// Releasing Behavior
		switch (instrInfo->relBehavior) {
			case SILInstruction::ReleasingBehavior::DoesNotRelease: {
				outs 	<< "\t\t [REL]: Does not release memory. \n";
				break;
			}
			case SILInstruction::ReleasingBehavior::MayRelease: {
				outs 	<< "\t\t [REL]: May release memory. \n";
				break;
			}
		}
	
		// Show operands, if they exist
		for (auto op = instrInfo->ops.begin(); op != instrInfo->ops.end(); ++op) {
			outs << "\t\t [OPER]: " << *op;
		}

		outs << "\n";	
	}
}