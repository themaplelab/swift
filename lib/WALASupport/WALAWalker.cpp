#include <stdlib.h>
#include <jni.h>
#include <csetjmp>
#include <fstream>
#include <string>

#include "swift/SIL/SILLocation.h"
#include "swift/Demangling/Demangle.h"
#include "swift/WALASupport/WALAWalker.h"

#include "launch.h"
#include "Exceptions.h"
#include "CAstWrapper.h"

using namespace swift;

//
// 	WALAIntegration:
// 		Methods for passing information to and from WALA.
//
class WALAIntegration {
public:
	void print_object(JNIEnv *java_env, jobject object) {
		jclass Object = java_env->FindClass("java/lang/Object");
	  
		jmethodID toString = java_env->GetMethodID(Object, "toString", "()Ljava/lang/String;");

		jstring msg = (jstring) java_env->CallObjectMethod(object, toString);

		jboolean f = false;
		const char *text = java_env->GetStringUTFChars(msg, &f);
	  
		printf("FOO: %s\n", text);
	  
		java_env->ReleaseStringUTFChars(msg, text);
	}

};

//
//	WALAWalker methods
//

// Gets the mangled and demangled SILFunction and returns in a FunctionInfo.
WALAWalker::FunctionInfo WALAWalker::getSILFunctionInfo(SILFunction &func) {

	WALAWalker::FunctionInfo funcInfo;
	funcInfo.name = func.getName();
	StringRef demangled = Demangle::demangleSymbolAsString(funcInfo.name);
	funcInfo.demangled = demangled;

	return funcInfo;
}

// Gets the sourcefile, start line/col, end line/col, and returns it in a 
// SourceRangeInfo.  If there is no valid loc, startLine and startCol are -1.
// If there was a valid loc, but info could not be pulled, it will have start
// info but endLine and endCol will be -1.
WALAWalker::SourceRangeInfo WALAWalker::getInstrSrcInfo(SILInstruction &instr) {
	
	SourceManager &srcMgr = instr.getModule().getSourceManager();
	
	// Get file-line-col information for the source
	SILLocation debugLoc = instr.getDebugLocation().getLocation();
	SILLocation::DebugLoc debugInfo = debugLoc.decodeDebugLoc(srcMgr);
	
	SourceRangeInfo srcInfo;
	srcInfo.Filename = debugInfo.Filename;
	
	if (instr.getLoc().isNull()) {
		srcInfo.type = -1;
	} else {

		SourceRange srcRange = instr.getLoc().getSourceRange();
		SourceLoc srcStart = srcRange.Start;
		SourceLoc srcEnd = srcRange.End;
		
		if (srcStart.isValid() ) {

			srcInfo.type = 0;
			
			auto startLineCol = srcMgr.getLineAndColumn(srcStart);	
			srcInfo.startLine = startLineCol.first;
			srcInfo.startCol = startLineCol.second;			

		} else {
			srcInfo.startLine = debugInfo.Line;
			srcInfo.startCol = debugInfo.Column;
		}
		
		if (srcEnd.isValid() ) {
		
			auto endLineCol = srcMgr.getLineAndColumn(srcEnd);
			srcInfo.endLine = endLineCol.first;
			srcInfo.endCol = endLineCol.second;
		} else {
			srcInfo.type = 1;
		}
	}
	
	return srcInfo;
}

// The big one - gets the ValueKind of the SILInstruction then goes through the
// mega-switch to handle appropriately.
ValueKind WALAWalker::getInstrValueKindInfo(SILInstruction &instr) {

	auto instrKind = instr.getKind();
	switch (instrKind) {
	
		case ValueKind::SILPHIArgument:
		case ValueKind::SILFunctionArgument:
		case ValueKind::SILUndef: {		
// 			outfile		<< "\t\t << Not an instruction >>" << "\n";
			break;
		}
		
		case ValueKind::AllocBoxInst: {
// 			outfile		<< "\t\t << AllocBoxInst >>" << "\n";
			break;
		}
	
		case ValueKind::ApplyInst: {
		
// 			outfile 	<< "\t\t << ApplyInst >>" << "\n";
		
			// Cast the instr 
// 			ApplyInst *castInst = cast<ApplyInst>(&instr);
			
			// Iterate args and output SILValue
// 			for (unsigned i = 0; i < applyInst->getNumArguments(); ++i) {
// 				SILValue v = applyInst->getArgument(i);
// 				outfile 	<< "\t\t\t\t [ARG] #" << i << ": " << v;
// 			}
			break;
		}
		
		case ValueKind::PartialApplyInst: {
// 			outfile		<< "\t\t << PartialApplyInst >>" << "\n";
			break;
		}
		
		case ValueKind::IntegerLiteralInst: {
// 			outfile		<< "\t\t << IntegerLiteralInst >>" << "\n";
			break;
		}
		
		case ValueKind::FloatLiteralInst: {
// 			outfile		<< "\t\t << FloatLiteralInst >>" << "\n";
			break;
		}
		
		case ValueKind::StringLiteralInst: {
// 			outfile		<< "\t\t << StringLiteralInst >>" << "\n";
			break;
		}
		
		case ValueKind::ConstStringLiteralInst: {
// 			outfile		<< "\t\t << ConstStringLiteralInst >>" << "\n";
			break;
		}
		
		case ValueKind::AllocValueBufferInst: {
// 			outfile		<< "\t\t << AllocValueBufferInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectValueBufferInst: {
// 			outfile		<< "\t\t << ProjectValueBufferInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocValueBufferInst: {
// 			outfile		<< "\t\t << DeallocValueBufferInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectBoxInst: {
// 			outfile		<< "\t\t << ProjectBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectExistentialBoxInst: {
// 			outfile		<< "\t\t << ProjectExistentialBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::FunctionRefInst: {

			// Cast the instr to access methods
			FunctionRefInst *castInst = cast<FunctionRefInst>(&instr);

			// ValueKind identifier
// 			outfile		<< "\t\t << FunctionRefInst >>" << "\n";

			// Demangled FunctionRef name
// 			outfile		<< "\t\t === [FUNC] Ref'd: ";
			string funcName = Demangle::demangleSymbolAsString(castInst->getReferencedFunction()->getName());
// 			outfile		<< 	funcName << "\n";
			break;
		}
		
		case ValueKind::BuiltinInst: {
// 			outfile		<< "\t\t << BuiltinInst >>" << "\n";
			break;
		}
		
		case ValueKind::OpenExistentialAddrInst:
		case ValueKind::OpenExistentialBoxInst:
		case ValueKind::OpenExistentialBoxValueInst:
		case ValueKind::OpenExistentialMetatypeInst:
		case ValueKind::OpenExistentialRefInst:
		case ValueKind::OpenExistentialValueInst: {
// 			outfile		<< "\t\t << OpenExistential[Addr/Box/BoxValue/Metatype/Ref/Value]Inst >>" << "\n";
			break;
		}
		
		// UNARY_INSTRUCTION(ID) <see ParseSIL.cpp:2248>
		// DEFCOUNTING_INSTRUCTION(ID) <see ParseSIL.cpp:2255>
		
		case ValueKind::DebugValueInst: {
// 			outfile		<< "\t\t << DebugValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::DebugValueAddrInst: {
// 			outfile		<< "\t\t << DebugValueAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::UncheckedOwnershipConversionInst: {
// 			outfile		<< "\t\t << UncheckedOwnershipConversionInst >>" << "\n";
			break;
		}
		
		case ValueKind::LoadInst: {
// 			outfile		<< "\t\t << LoadInst >>" << "\n";
			break;
		}
		
		case ValueKind::LoadBorrowInst: {
// 			outfile		<< "\t\t << LoadBorrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::BeginBorrowInst: {
// 			outfile		<< "\t\t << BeginBorrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::LoadUnownedInst: {
// 			outfile		<< "\t\t << LoadUnownedInst >>" << "\n";
			break;
		}
		
		case ValueKind::LoadWeakInst: {
// 			outfile		<< "\t\t << LoadWeakInst >>" << "\n";
			break;
		}
		
		case ValueKind::MarkDependenceInst: {
// 			outfile		<< "\t\t << MarkDependenceInst >>" << "\n";
			break;
		}
		
		case ValueKind::KeyPathInst: {
// 			outfile		<< "\t\t << KeyPathInst >>" << "\n";
			break;
		}
		
		case ValueKind::UncheckedRefCastInst:
		case ValueKind::UncheckedAddrCastInst:
		case ValueKind::UncheckedTrivialBitCastInst:
		case ValueKind::UncheckedBitwiseCastInst:
		case ValueKind::UpcastInst:
		case ValueKind::AddressToPointerInst:
		case ValueKind::BridgeObjectToRefInst:
		case ValueKind::BridgeObjectToWordInst:
		case ValueKind::RefToRawPointerInst:
		case ValueKind::RawPointerToRefInst:
		case ValueKind::RefToUnownedInst:
		case ValueKind::UnownedToRefInst:
		case ValueKind::RefToUnmanagedInst:
		case ValueKind::UnmanagedToRefInst:
		case ValueKind::ThinFunctionToPointerInst:
		case ValueKind::PointerToThinFunctionInst:
		case ValueKind::ThinToThickFunctionInst:
		case ValueKind::ThickToObjCMetatypeInst:
		case ValueKind::ObjCToThickMetatypeInst:
		case ValueKind::ConvertFunctionInst:
		case ValueKind::ObjCExistentialMetatypeToObjectInst:
		case ValueKind::ObjCMetatypeToObjectInst: {
// 			outfile		<< "\t\t << Conversion Instruction >>" << "\n";
  			break;
  		}
  		
  		case ValueKind::PointerToAddressInst: {
// 			outfile		<< "\t\t << PointerToAddressInst >>" << "\n";
			break;
		}
		
		case ValueKind::RefToBridgeObjectInst: {
// 			outfile		<< "\t\t << RefToBridgeObjectInst >>" << "\n";
			break;
		}
		
		case ValueKind::UnconditionalCheckedCastAddrInst:
		case ValueKind::CheckedCastAddrBranchInst:
		case ValueKind::UncheckedRefCastAddrInst: {
// 			outfile		<< "\t\t << Indirect checked conversion instruction >>" << "\n";
			break;
		}
		
		case ValueKind::UnconditionalCheckedCastValueInst: {
// 			outfile		<< "\t\t << UnconditionalCheckedCastValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::UnconditionalCheckedCastInst:
		case ValueKind::CheckedCastValueBranchInst:
		case ValueKind::CheckedCastBranchInst: {
// 			outfile		<< "\t\t << Checked conversion instruction >>" << "\n";
			break;
		}
		
		case ValueKind::MarkUninitializedInst: {
// 			outfile		<< "\t\t << MarkUninitializedInst >>" << "\n";
			break;
		}
		
		case ValueKind::MarkUninitializedBehaviorInst: {
// 			outfile		<< "\t\t << MarkUninitializedBehaviorInst >>" << "\n";
			break;
		}
		
		case ValueKind::MarkFunctionEscapeInst: {
// 			outfile		<< "\t\t << MarkFunctionEscapeInst >>" << "\n";
			break;
		}
		
		case ValueKind::StoreInst: {
// 			outfile		<< "\t\t << StoreInst >>" << "\n";
			break;
		}
		
		case ValueKind::EndBorrowInst: {
// 			outfile		<< "\t\t << EndBorrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::BeginAccessInst:
		case ValueKind::BeginUnpairedAccessInst:
		case ValueKind::EndAccessInst:
		case ValueKind::EndUnpairedAccessInst: {
// 			outfile		<< "\t\t << Access Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::StoreBorrowInst:
		case ValueKind::AssignInst:
		case ValueKind::StoreUnownedInst:
		case ValueKind::StoreWeakInst: {
// 			outfile		<< "\t\t << Access Instruction >>" << "\n";
			break;
		}

		case ValueKind::AllocStackInst: {
// 			outfile		<< "\t\t << AllocStack Instruction >>" << "\n";
			break;
		}
		case ValueKind::MetatypeInst: {		
// 			outfile		<< "\t\t << MetatypeInst >>" << "\n";
			break;
		}
		
		case ValueKind::AllocRefInst:
		case ValueKind::AllocRefDynamicInst: {
// 			outfile		<< "\t\t << Alloc[Ref/RefDynamic] Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocStackInst: {		
// 			outfile		<< "\t\t << DeallocStackInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocRefInst: {		
// 			outfile		<< "\t\t << DeallocRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocPartialRefInst: {		
// 			outfile		<< "\t\t << DeallocPartialRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocBoxInst: {		
// 			outfile		<< "\t\t << DeallocBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::ValueMetatypeInst: 
		case ValueKind::ExistentialMetatypeInst: {		
// 			outfile		<< "\t\t << [Value/Existential]MetatypeInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocExistentialBoxInst: {		
// 			outfile		<< "\t\t << DeallocExistentialBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::TupleInst: {		
// 			outfile		<< "\t\t << TupleInst >>" << "\n";
			break;
		}
		
		case ValueKind::EnumInst: {		
// 			outfile		<< "\t\t << EnumInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitEnumDataAddrInst:
		case ValueKind::UncheckedEnumDataInst:
		case ValueKind::UncheckedTakeEnumDataAddrInst: {		
// 			outfile		<< "\t\t << EnumData Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::InjectEnumAddrInst: {		
// 			outfile		<< "\t\t << InjectEnumAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::TupleElementAddrInst:
		case ValueKind::TupleExtractInst: {		
// 			outfile		<< "\t\t << Tuple Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::ReturnInst: {		
// 			outfile		<< "\t\t << ReturnInst >>" << "\n";
			break;
		}
		
		case ValueKind::ThrowInst: {		
// 			outfile		<< "\t\t << ThrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::BranchInst: {		
// 			outfile		<< "\t\t << BranchInst >>" << "\n";
			break;
		}
		
		case ValueKind::CondBranchInst: {		
// 			outfile		<< "\t\t << CondBranchInst >>" << "\n";
			break;
		}
		
		case ValueKind::UnreachableInst: {		
// 			outfile		<< "\t\t << UnreachableInst >>" << "\n";
			break;
		}
		
		case ValueKind::ClassMethodInst:
		case ValueKind::SuperMethodInst:
		case ValueKind::DynamicMethodInst: {		
// 			outfile		<< "\t\t << DeallocRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::WitnessMethodInst: {		
// 			outfile		<< "\t\t << WitnessMethodInst >>" << "\n";
			break;
		}
		
		case ValueKind::CopyAddrInst: {		
// 			outfile		<< "\t\t << CopyAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::BindMemoryInst: {		
// 			outfile		<< "\t\t << BindMemoryInst >>" << "\n";
			break;
		}
		
		case ValueKind::StructInst: {		
// 			outfile		<< "\t\t << StructInst >>" << "\n";
			break;
		}
		
		case ValueKind::StructElementAddrInst:
		case ValueKind::StructExtractInst: {		
// 			outfile		<< "\t\t << Struct Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::RefElementAddrInst: {		
// 			outfile		<< "\t\t << RefElementAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::RefTailAddrInst: {		
// 			outfile		<< "\t\t << RefTailAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::IsNonnullInst: {		
// 			outfile		<< "\t\t << IsNonnullInst >>" << "\n";
			break;
		}
		
		case ValueKind::IndexAddrInst: {		
// 			outfile		<< "\t\t << IndexAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::TailAddrInst: {		
// 			outfile		<< "\t\t << TailAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::IndexRawPointerInst: {		
// 			outfile		<< "\t\t << IndexRawPointerInst >>" << "\n";
			break;
		}
		
		case ValueKind::ObjCProtocolInst: {		
// 			outfile		<< "\t\t << ObjCProtocolInst >>" << "\n";
			break;
		}
		
		case ValueKind::AllocGlobalInst: {		
// 			outfile		<< "\t\t << AllocGlobalInst >>" << "\n";
			break;
		}
		
		case ValueKind::GlobalAddrInst: {		
// 			outfile		<< "\t\t << GlobalAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::SelectEnumInst: {		
// 			outfile		<< "\t\t << SelectEnumInst >>" << "\n";
			break;
		}
		
		case ValueKind::SelectEnumAddrInst: {		
// 			outfile		<< "\t\t << DeallocRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::SwitchEnumInst: {		
// 			outfile		<< "\t\t << SwitchEnumInst >>" << "\n";
			break;
		}
		
		case ValueKind::SwitchEnumAddrInst: {		
// 			outfile		<< "\t\t << SwitchEnumAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::SwitchValueInst: {		
// 			outfile		<< "\t\t << SwitchValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::SelectValueInst: {		
// 			outfile		<< "\t\t << SelectValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeinitExistentialAddrInst: {		
// 			outfile		<< "\t\t << DeinitExistentialAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeinitExistentialValueInst: {		
// 			outfile		<< "\t\t << DeinitExistentialValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialAddrInst: {		
// 			outfile		<< "\t\t << InitExistentialAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialValueInst: {		
// 			outfile		<< "\t\t << InitExistentialValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::AllocExistentialBoxInst: {		
// 			outfile		<< "\t\t << AllocExistentialBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialRefInst: {		
// 			outfile		<< "\t\t << InitExistentialRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialMetatypeInst: {		
// 			outfile		<< "\t\t << InitExistentialMetatypeInst >>" << "\n";
			break;
		}
		
		case ValueKind::DynamicMethodBranchInst: {		
// 			outfile		<< "\t\t << DynamicMethodBranchInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectBlockStorageInst: {		
// 			outfile		<< "\t\t << ProjectBlockStorageInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitBlockStorageHeaderInst: {		
// 			outfile		<< "\t\t << InitBlockStorageHeaderInst >>" << "\n";
			break;
		}		
		
		default: {
// 			outfile 	<< "\t\t\t xxxxx Not a handled inst type \n";
			break;
		}
	}
	
	return instrKind;
}

// Main WALAWalker implementation.
void WALAWalker::analyzeSILModule(SILModule &SM) {

	// Iterate over SILFunctions
	for (auto func = SM.begin(); func != SM.end(); ++func) {
	
		WALAWalker::FunctionInfo funcInfo = getSILFunctionInfo(*func);
	
		// Iterate over SILBasicBlocks
		for (auto bb = func->begin(); bb != func->end(); ++bb) {
		
			unsigned i = 0; 	// for Instruction count
			
			// Iterate over SILInstructions
			for (auto instr = bb->begin(); instr != bb->end(); ++instr) {
				
				WALAWalker::InstrInfo instrInfo;
				instrInfo.num = i;
				instrInfo.funcInfo = &funcInfo;
				WALAWalker::SourceRangeInfo srcInfo = getInstrSrcInfo(*instr);
				instrInfo.srcInfo = &srcInfo;
				instrInfo.instrKind = getInstrValueKindInfo(*instr);
				
				perInstruction(SM, instrInfo);
				
				++i;
			
			}	// end SILInstruction
		}	// end SILBasicBlock
	}	// end SILFunction
}

// Do something per instruction
void WALAWalker::perInstruction(SILModule &SM, WALAWalker::InstrInfo instrInfo) {

	raw_ostream &outs = llvm::outs();
	WALAWalker::SourceRangeInfo src = *(instrInfo.srcInfo);	

	outs << "\t [INSTR] #" << instrInfo.num << ": \n";
	outs << "\t --> File: " << src.Filename << "\n";
	if (src.type == -1) {
		outs << "\t **** No source information. \n";
	} else {
		outs << "\t ++++ Start - Line " << src.startLine << ":" 
			<< src.startCol << "\n";
	}
	if (src.type == 0) {
		outs << "\t ---- End - Line " << src.endLine << ":" << src.endCol << "\n";
	}
}

void WALAWalker::foo() {
  char *walaHome = getenv("WALA_HOME");
  char *swiftWalaHome = getenv("SWIFT_WALA_HOME");
  char classpath[1024];
  sprintf(classpath, "%s/com.ibm.wala.util/target/classes:%s/com.ibm.wala.shrike/target/classes:%s/com.ibm.wala.core/target/classes:%s/com.ibm.wala.cast/target/classes:%s/com.ibm.wala.cast.swift/bin", walaHome, walaHome, walaHome, walaHome, swiftWalaHome);

  // Instance the WALAIntegration object to call print_object later
  WALAIntegration wala;

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

      wala.print_object(java_env, x);
      THROW_ANY_EXCEPTION(cpp_ex);
      
  START_CATCH_BLOCK()

      if (java_env->ExceptionCheck()) {
	  jthrowable real_ex = java_env->ExceptionOccurred();
	  
	  wala.print_object(java_env, real_ex);
      }
  
  END_CATCH_BLOCK()
}
