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

jobject WALAIntegration::makeConstant(string value) {
	return CAst->makeConstant(value.c_str());
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
ValueKind WALAWalker::getInstrValueKindInfo(SILInstruction &instr, WALAIntegration &wala) {
	raw_ostream &outs = llvm::outs();

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

			// Cast the instr to access methods
			StringLiteralInst *castInst = cast<StringLiteralInst>(&instr);

			// ValueKind indentifier
 			outs		<< "\t\t << StringLiteralInst >>" << "\n";

 			// Value: the string data for the literal, in UTF-8.
			StringRef value = castInst->getValue();
			outs     << "\t\t\t\t [VAL]: " << value << "\n";

			// Encoding: the desired encoding of the text.
			string encoding;
			switch (castInst->getEncoding()) {
				case StringLiteralInst::Encoding::UTF8: {
					encoding = "UTF8";
					break;
				}
				case StringLiteralInst::Encoding::UTF16: {
					encoding = "UTF16";
					break;
				}
				case StringLiteralInst::Encoding::ObjCSelector: {
					encoding = "ObjCSelector";
					break;
				}
			}
			outs     << "\t\t\t\t [ENCODING]: " << encoding << "\n";

			// Count: encoding-based length of the string literal in code units.
			uint64_t codeUnitCount = castInst->getCodeUnitCount();
			outs     << "\t\t\t\t [COUNT]: " << codeUnitCount << "\n";

			// Call WALA in Java
			jobject walaConstant = wala.makeConstant(value);
			wala.print(walaConstant);
			break;
		}
		
		case ValueKind::ConstStringLiteralInst: {
 			outfile		<< "\t\t << ConstStringLiteralInst >>" << "\n";
			// Cast the instr to access methods
			ConstStringLiteralInst *castInst = cast<ConstStringLiteralInst>(&instr);
			 			// Value: the string data for the literal, in UTF-8.
			StringRef value = castInst->getValue();
			outs     << "\t\t\t\t [VAL]: " << value << "\n";

			// Encoding: the desired encoding of the text.
			string encoding;
			switch (castInst->getEncoding()) {
				case StringLiteralInst::Encoding::UTF8: {
					encoding = "UTF8";
					break;
				}
				case StringLiteralInst::Encoding::UTF16: {
					encoding = "UTF16";
					break;
				}
				case StringLiteralInst::Encoding::ObjCSelector: {
					encoding = "ObjCSelector";
					break;
				}
			}
			outs     << "\t\t\t\t [ENCODING]: " << encoding << "\n";

			// Count: encoding-based length of the string literal in code units.
			uint64_t codeUnitCount = castInst->getCodeUnitCount();
			outs     << "\t\t\t\t [COUNT]: " << codeUnitCount << "\n";

			// Call WALA in Java
			jobject walaConstant = wala.makeConstant(value);
			wala.print(walaConstant);
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
	
	JNIEnv *java_env = launch(classpath);
	TRY(cpp_ex, java_env)		
		WALAIntegration wala(java_env, cpp_ex, file_name);
		jobject x = wala->makeConstant(3.7);
		wala.print(x);
	
	// Iterate over SILFunctions
	for (auto func = SM.begin(); func != SM.end(); ++func) {
	
		FunctionInfo funcInfo = getSILFunctionInfo(*func);
	
		// Iterate over SILBasicBlocks
		for (auto bb = func->begin(); bb != func->end(); ++bb) {
		
			unsigned i = 0; 	// for Instruction count
			
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
				instrInfo.instrKind = getInstrValueKindInfo(*instr, wala);

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
		}	// end SILBasicBlock
	}	// end SILFunction
	
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
