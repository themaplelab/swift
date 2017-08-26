#include <stdlib.h>
#include <jni.h>
#include <csetjmp>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>

#include "swift/SIL/SILLocation.h"
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

// Takes the shortFilename, concatenates the $SWIFT_WALA_OUTPUT dir, and writes the result
// to char *outfileName.  Used for dump() and open().
void getOutputFilename(raw_ostream &outstream, string filenamePath, 
	char *outfileName) {
	// Get output dir	
	string outputDir = getenv("SWIFT_WALA_OUTPUT");

	// Get shortfilename (trailing final '/')	
	string splitString = "/";
	size_t splitPoint = filenamePath.find_last_of(splitString);
	string shortFilename = filenamePath.substr(splitPoint + 1);
	if (shortFilename.empty()) {
		shortFilename = "_noSourceFileFound";
	} else {
		// Get rid of the .swift extension
		string swiftExt = ".swift";
		string newExt = "";
		size_t ext = shortFilename.find(swiftExt);
		shortFilename.replace(swiftExt, swiftExt.length(), newExt);
	}
	
	// Concatenate to output full path
	sprintf(outfileName, "%s/%s.txt", outputDir.c_str(), shortFilename.c_str());

	// Check if file exists; if so, try to make the file unique
	unsigned i = 0;
	ifstream fileCheck;
	fileCheck.open(outfileName);
	while (fileCheck && i < 100) {
		fileCheck.close();
		sprintf(outfileName, "%s/%s_%u.txt", outputDir.c_str(), shortFilename.c_str(), i);
		fileCheck.open(outfileName);
		i++;
	}

	fileCheck.close();
	
	// DEBUG OUTSTREAM
	outstream << "\t [OUTPUTFILE]: " << outfileName << "\n";
	outstream << "\t [FILENAME]: " << shortFilename << ".txt" << "\n";
}

// Prints the path (after swift/) to outfile if it is open and writeable.
void printSourceFilepath(llvm::raw_fd_ostream &outfile, string filenamePath) {
	size_t splitPoint = filenamePath.find("swift/");
	if (splitPoint == string::npos) {
		// 
	} else {
		string shortPath = filenamePath.substr(splitPoint);
		outfile 	<< "\t [SOURCE] file: " << shortPath << "\n\n";
	}
}

// Prints the SIL to the outfile if it is open and writeable.
void printSIL(char *outputFilename, SILModule &SM) {
	
	// Convert <filename>.txt to <filename>.sil
	string strFilename(outputFilename);
	string extToReplace = ".txt";
	string newExt = ".sil";
 	size_t splitPoint = strFilename.find(extToReplace);
	const char *SILFilename = strFilename.replace(splitPoint, extToReplace.length(), newExt).c_str();
	
	// DEBUG OUTSTREAM
	llvm::outs() << "\t [SIL]: " << SILFilename << "\n";

	// Output to .sil
	SM.dump(SILFilename);
}

// Outputs the mangled SILFunction name to outstream.
void printSILFunctionInfo(llvm::raw_fd_ostream &outfile, SILFunction &func) {

	// Output function name to written file
	outfile 	<< "\t -- [FUNCTION] Name: " << func.getName() << "\n";	
}

// Outputs the SILBasicBlock ID to outstream.
void printSILBasicBlockInfo(llvm::raw_fd_ostream &outfile, SILBasicBlock &bb) {

	// Print SILBasicBlock ID
	SILPrintContext context(outfile);
	SILPrintContext::ID bbID = context.getID(&bb);
	outfile 	<< "\t ---- [BASIC BLOCK] ID: " << bbID << "\n";
}

// Prints the sourcefile, line, and column info to outstream.
void printInstrDebugLocInfo(llvm::raw_fd_ostream &outfile,
	SILInstruction &instr, SourceManager &srcMgr) {

	// Get file-line-col information for the source
	SILLocation debugLoc = instr.getDebugLocation().getLocation();
	SILLocation::DebugLoc debugInfo = debugLoc.decodeDebugLoc(srcMgr);
	
	size_t splitPoint = debugInfo.Filename.find("swift/");
	string splitString = debugInfo.Filename.substr(splitPoint);
	
	if (debugInfo.Filename == "") {
		outfile << "\t\t\t >>> Source: <no file>";
	} else {
		outfile << "\t\t\t >>> Source: " << splitString;
	}
	outfile << ", Line: " << debugInfo.Line;
	outfile << ", Col: " << debugInfo.Column << "\n";
}

// Outputs to outstream whether instr may release, write to memory, read from memory,
// trap the instruction, or potentially have side effects.
void printInstrMemoryReleasingInfo(llvm::raw_fd_ostream &outfile, SILInstruction &instr) {

	switch (instr.getMemoryBehavior()) {
		case SILInstruction::MemoryBehavior::None: {
			break;
		}
		case SILInstruction::MemoryBehavior::MayRead: {
			outfile 	<< "\t\t\t +++ [MEM-R]: May read from memory. \n";
			break;
		}
		case SILInstruction::MemoryBehavior::MayWrite: {
			outfile 	<< "\t\t\t +++ [MEM-W]: May write to memory. \n";
			break;
		}
		case SILInstruction::MemoryBehavior::MayReadWrite: {
			outfile 	<< "\t\t\t +++ [MEM-RW]: May read or write memory. \n";
			break;
		}
		case SILInstruction::MemoryBehavior::MayHaveSideEffects: {
			outfile 	<< "\t\t\t +++ [MEM-F]: May have side effects. \n";
		}
	}
	
	switch (instr.getReleasingBehavior()) {
		case SILInstruction::ReleasingBehavior::DoesNotRelease: {
			outfile 	<< "\t\t\t +++ [REL]: Does not release memory. \n";
			break;
		}
		case SILInstruction::ReleasingBehavior::MayRelease: {
			outfile 	<< "\t\t\t +++ [REL]: May release memory. \n";
			break;
		}
	}
}

// Goes over all operands on the SILInstr and prints them out.
void printInstrOpInfo(llvm::raw_fd_ostream &outfile, SILInstruction &instr) {

	if (instr.getNumOperands() == 0) {
		outfile		<< "\t\t\t [OPER]: No Operands." << "\n";
	} else {

		// Output operand information
		for (unsigned i = 0; i < instr.getNumOperands(); ++i) {
			SILValue v = instr.getOperand(i);
			outfile 	<< "\t\t\t *** [OPER] #" << i << ": " << v;
		}
	}
}

// The big one - gets the ValueKind of the SILInstruction then goes through the
// mega-switch to handle appropriately.
void printInstrValueKindInfo(llvm::raw_fd_ostream &outfile, SILInstruction &instr) {

	//
	auto instrKind = instr.getKind();
	switch (instrKind) {
	
		case ValueKind::SILPHIArgument:
		case ValueKind::SILFunctionArgument:
		case ValueKind::SILUndef: {		
			outfile		<< "\t\t << Not an instruction >>" << "\n";
			break;
		}
		
		case ValueKind::AllocBoxInst: {		
			outfile		<< "\t\t << AllocBoxInst >>" << "\n";
			break;
		}
	
		case ValueKind::ApplyInst: {
		
			outfile 	<< "\t\t << ApplyInst >>" << "\n";
		
			// Cast to ValueKind::ApplyInst 
// 			ApplyInst *applyInst = cast<ApplyInst>(&instr);
			
			// Iterate args and output SILValue
// 			for (unsigned i = 0; i < applyInst->getNumArguments(); ++i) {
// 				SILValue v = applyInst->getArgument(i);
// 				outfile 	<< "\t\t\t\t [ARG] #" << i << ": " << v;
// 			}
			break;
		}
		
		case ValueKind::PartialApplyInst: {
			outfile		<< "\t\t << PartialApplyInst >>" << "\n";
			break;
		}
		
		case ValueKind::IntegerLiteralInst: {
			outfile		<< "\t\t << IntegerLiteralInst >>" << "\n";
			break;
		}
		
		case ValueKind::FloatLiteralInst: {
			outfile		<< "\t\t << FloatLiteralInst >>" << "\n";
			break;
		}
		
		case ValueKind::StringLiteralInst: {
			outfile		<< "\t\t << StringLiteralInst >>" << "\n";
			break;
		}
		
		case ValueKind::ConstStringLiteralInst: {
			outfile		<< "\t\t << ConstStringLiteralInst >>" << "\n";
			break;
		}
		
		case ValueKind::AllocValueBufferInst: {
			outfile		<< "\t\t << AllocValueBufferInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectValueBufferInst: {
			outfile		<< "\t\t << ProjectValueBufferInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocValueBufferInst: {
			outfile		<< "\t\t << DeallocValueBufferInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectBoxInst: {
			outfile		<< "\t\t << ProjectBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectExistentialBoxInst: {
			outfile		<< "\t\t << ProjectExistentialBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::FunctionRefInst: {
			outfile		<< "\t\t << FunctionRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::BuiltinInst: {
			outfile		<< "\t\t << BuiltinInst >>" << "\n";
			break;
		}
		
		case ValueKind::OpenExistentialAddrInst:
		case ValueKind::OpenExistentialBoxInst:
		case ValueKind::OpenExistentialBoxValueInst:
		case ValueKind::OpenExistentialMetatypeInst:
		case ValueKind::OpenExistentialRefInst:
		case ValueKind::OpenExistentialValueInst: {
			outfile		<< "\t\t << OpenExistential[Addr/Box/BoxValue/Metatype/Ref/Value]Inst >>" << "\n";
			break;
		}
		
		// UNARY_INSTRUCTION(ID) <see ParseSIL.cpp:2248>
		// DEFCOUNTING_INSTRUCTION(ID) <see ParseSIL.cpp:2255>
		
		case ValueKind::DebugValueInst: {
			outfile		<< "\t\t << DebugValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::DebugValueAddrInst: {
			outfile		<< "\t\t << DebugValueAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::UncheckedOwnershipConversionInst: {
			outfile		<< "\t\t << UncheckedOwnershipConversionInst >>" << "\n";
			break;
		}
		
		case ValueKind::LoadInst: {
			outfile		<< "\t\t << LoadInst >>" << "\n";
			break;
		}
		
		case ValueKind::LoadBorrowInst: {
			outfile		<< "\t\t << LoadBorrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::BeginBorrowInst: {
			outfile		<< "\t\t << BeginBorrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::LoadUnownedInst: {
			outfile		<< "\t\t << LoadUnownedInst >>" << "\n";
			break;
		}
		
		case ValueKind::LoadWeakInst: {
			outfile		<< "\t\t << LoadWeakInst >>" << "\n";
			break;
		}
		
		case ValueKind::MarkDependenceInst: {
			outfile		<< "\t\t << MarkDependenceInst >>" << "\n";
			break;
		}
		
		case ValueKind::KeyPathInst: {
			outfile		<< "\t\t << KeyPathInst >>" << "\n";
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
			outfile		<< "\t\t << Conversion Instruction >>" << "\n";
  			break;
  		}
  		
  		case ValueKind::PointerToAddressInst: {
			outfile		<< "\t\t << PointerToAddressInst >>" << "\n";
			break;
		}
		
		case ValueKind::RefToBridgeObjectInst: {
			outfile		<< "\t\t << RefToBridgeObjectInst >>" << "\n";
			break;
		}
		
		case ValueKind::UnconditionalCheckedCastAddrInst:
		case ValueKind::CheckedCastAddrBranchInst:
		case ValueKind::UncheckedRefCastAddrInst: {
			outfile		<< "\t\t << Indirect checked conversion instruction >>" << "\n";
			break;
		}
		
		case ValueKind::UnconditionalCheckedCastValueInst: {
			outfile		<< "\t\t << UnconditionalCheckedCastValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::UnconditionalCheckedCastInst:
		case ValueKind::CheckedCastValueBranchInst:
		case ValueKind::CheckedCastBranchInst: {
			outfile		<< "\t\t << Checked conversion instruction >>" << "\n";
			break;
		}
		
		case ValueKind::MarkUninitializedInst: {
			outfile		<< "\t\t << MarkUninitializedInst >>" << "\n";
			break;
		}
		
		case ValueKind::MarkUninitializedBehaviorInst: {
			outfile		<< "\t\t << MarkUninitializedBehaviorInst >>" << "\n";
			break;
		}
		
		case ValueKind::MarkFunctionEscapeInst: {
			outfile		<< "\t\t << MarkFunctionEscapeInst >>" << "\n";
			break;
		}
		
		case ValueKind::StoreInst: {
			outfile		<< "\t\t << StoreInst >>" << "\n";
			break;
		}
		
		case ValueKind::EndBorrowInst: {
			outfile		<< "\t\t << EndBorrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::BeginAccessInst:
		case ValueKind::BeginUnpairedAccessInst:
		case ValueKind::EndAccessInst:
		case ValueKind::EndUnpairedAccessInst: {
			outfile		<< "\t\t << Access Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::StoreBorrowInst:
		case ValueKind::AssignInst:
		case ValueKind::StoreUnownedInst:
		case ValueKind::StoreWeakInst: {
			outfile		<< "\t\t << Access Instruction >>" << "\n";
			break;
		}

		case ValueKind::AllocStackInst: {
			outfile		<< "\t\t << AllocStack Instruction >>" << "\n";
			break;
		}
		case ValueKind::MetatypeInst: {		
			outfile		<< "\t\t << Metatype Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::AllocRefInst:
		case ValueKind::AllocRefDynamicInst: {
			outfile		<< "\t\t << Alloc[Ref/RefDynamic] Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocStackInst: {		
			outfile		<< "\t\t << DeallocStackInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocRefInst: {		
			outfile		<< "\t\t << DeallocRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocPartialRefInst: {		
			outfile		<< "\t\t << DeallocPartialRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocBoxInst: {		
			outfile		<< "\t\t << DeallocBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::ValueMetatypeInst: 
		case ValueKind::ExistentialMetatypeInst: {		
			outfile		<< "\t\t << [Value/Existential]MetatypeInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocExistentialBoxInst: {		
			outfile		<< "\t\t << DeallocExistentialBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::TupleInst: {		
			outfile		<< "\t\t << TupleInst >>" << "\n";
			break;
		}
		
		case ValueKind::EnumInst: {		
			outfile		<< "\t\t << EnumInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitEnumDataAddrInst:
		case ValueKind::UncheckedEnumDataInst:
		case ValueKind::UncheckedTakeEnumDataAddrInst: {		
			outfile		<< "\t\t << EnumData Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::InjectEnumAddrInst: {		
			outfile		<< "\t\t << InjectEnumAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::TupleElementAddrInst:
		case ValueKind::TupleExtractInst: {		
			outfile		<< "\t\t << Tuple Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::ReturnInst: {		
			outfile		<< "\t\t << ReturnInst >>" << "\n";
			break;
		}
		
		case ValueKind::ThrowInst: {		
			outfile		<< "\t\t << ThrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::BranchInst: {		
			outfile		<< "\t\t << BranchInst >>" << "\n";
			break;
		}
		
		case ValueKind::CondBranchInst: {		
			outfile		<< "\t\t << CondBranchInst >>" << "\n";
			break;
		}
		
		case ValueKind::UnreachableInst: {		
			outfile		<< "\t\t << UnreachableInst >>" << "\n";
			break;
		}
		
		case ValueKind::ClassMethodInst:
		case ValueKind::SuperMethodInst:
		case ValueKind::DynamicMethodInst: {		
			outfile		<< "\t\t << DeallocRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::WitnessMethodInst: {		
			outfile		<< "\t\t << WitnessMethodInst >>" << "\n";
			break;
		}
		
		case ValueKind::CopyAddrInst: {		
			outfile		<< "\t\t << CopyAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::BindMemoryInst: {		
			outfile		<< "\t\t << BindMemoryInst >>" << "\n";
			break;
		}
		
		case ValueKind::StructInst: {		
			outfile		<< "\t\t << StructInst >>" << "\n";
			break;
		}
		
		case ValueKind::StructElementAddrInst:
		case ValueKind::StructExtractInst: {		
			outfile		<< "\t\t << Struct Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::RefElementAddrInst: {		
			outfile		<< "\t\t << RefElementAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::RefTailAddrInst: {		
			outfile		<< "\t\t << RefTailAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::IsNonnullInst: {		
			outfile		<< "\t\t << IsNonnullInst >>" << "\n";
			break;
		}
		
		case ValueKind::IndexAddrInst: {		
			outfile		<< "\t\t << IndexAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::TailAddrInst: {		
			outfile		<< "\t\t << TailAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::IndexRawPointerInst: {		
			outfile		<< "\t\t << IndexRawPointerInst >>" << "\n";
			break;
		}
		
		case ValueKind::ObjCProtocolInst: {		
			outfile		<< "\t\t << ObjCProtocolInst >>" << "\n";
			break;
		}
		
		case ValueKind::AllocGlobalInst: {		
			outfile		<< "\t\t << AllocGlobalInst >>" << "\n";
			break;
		}
		
		case ValueKind::GlobalAddrInst: {		
			outfile		<< "\t\t << GlobalAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::SelectEnumInst: {		
			outfile		<< "\t\t << SelectEnumInst >>" << "\n";
			break;
		}
		
		case ValueKind::SelectEnumAddrInst: {		
			outfile		<< "\t\t << DeallocRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::SwitchEnumInst: {		
			outfile		<< "\t\t << SwitchEnumInst >>" << "\n";
			break;
		}
		
		case ValueKind::SwitchEnumAddrInst: {		
			outfile		<< "\t\t << SwitchEnumAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::SwitchValueInst: {		
			outfile		<< "\t\t << SwitchValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::SelectValueInst: {		
			outfile		<< "\t\t << SelectValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeinitExistentialAddrInst: {		
			outfile		<< "\t\t << DeinitExistentialAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeinitExistentialValueInst: {		
			outfile		<< "\t\t << DeinitExistentialValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialAddrInst: {		
			outfile		<< "\t\t << InitExistentialAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialValueInst: {		
			outfile		<< "\t\t << InitExistentialValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::AllocExistentialBoxInst: {		
			outfile		<< "\t\t << AllocExistentialBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialRefInst: {		
			outfile		<< "\t\t << InitExistentialRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialMetatypeInst: {		
			outfile		<< "\t\t << InitExistentialMetatypeInst >>" << "\n";
			break;
		}
		
		case ValueKind::DynamicMethodBranchInst: {		
			outfile		<< "\t\t << DynamicMethodBranchInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectBlockStorageInst: {		
			outfile		<< "\t\t << ProjectBlockStorageInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitBlockStorageHeaderInst: {		
			outfile		<< "\t\t << InitBlockStorageHeaderInst >>" << "\n";
			break;
		}		
		
		default: {
			outfile 	<< "\t\t\t xxxxx Not a handled inst type \n";
			break;
		}
	}
}

// Handles all the SILInstruction printing and management.
void printSILInstrInfo(llvm::raw_fd_ostream &outfile,
	SILInstruction &instr, SourceManager &srcMgr) {
		
	printInstrDebugLocInfo(outfile, instr, srcMgr);
	printInstrMemoryReleasingInfo(outfile, instr);
	printInstrOpInfo(outfile, instr);
}

// Break down SILModule -> SILFunction -> SILBasicBlock -> SILInstruction -> SILValue
void getModBreakdown(llvm::raw_fd_ostream &outfile,
	SILModule &SM, SourceManager &srcMgr) {

	// Iterate over SILFunctions
	for (auto func = SM.begin(); func != SM.end(); ++func) {
	
		printSILFunctionInfo(outfile, *func);
	
		// Iterate over SILBasicBlocks
		for (auto bb = func->begin(); bb != func->end(); ++bb) {
		
			printSILBasicBlockInfo(outfile, *bb);
			unsigned i = 0; 	// for Instruction count
			
			for (auto instr = bb->begin(); instr != bb->end(); ++instr) {
			
				outfile 	<< "\t\t ----> [INSTR] #" << i << ":";
				printInstrValueKindInfo(outfile, *instr);

				printSILInstrInfo(outfile, *instr, srcMgr);
				outfile 	<< "\n";
				++i;
			
			}
			
			// End SILBasicBlock block
			outfile 	<< "\n";
		}
		
		// End SILFunction block
		outfile 	<< "\n";
	}
}

// Main WALAWalker implementation.
void analyzeSILModule(SILModule &SM) {

	// Modes and settings 
	bool outputSIL = true;		// Whether or not to create a full SIL dump to file

	// Output configurations
	raw_ostream &outstream = llvm::outs();
	
	// SILModule-derived information
	SourceManager &srcMgr = SM.getSourceManager();
	string filenamePath = SM.getSwiftModule()->getModuleFilename().str();
	
	// Get output filename
	char outputFilename[1024];
	getOutputFilename(outstream, filenamePath, outputFilename);
	
	// Open outfile as llvm::raw_fd_ostream for writing
	std::error_code EC;
	llvm::raw_fd_ostream outfile(outputFilename, EC, llvm::sys::fs::F_None);
	
	if (EC) {
      llvm::errs() << "[FILE]: Error opening  '" << outputFilename << "': "
                   << EC.message() << '\n';
	}	
	
	// Print and file-output source path information
	printSourceFilepath(outfile, filenamePath);

	// Dump SIL for SILModule to output file.
	if (outputSIL) { printSIL(outputFilename, SM); }
	outstream << "\n";
	
	// Iterate SILModule -> SILFunction -> SILBasicBlock -> SILInstruction
	getModBreakdown(outfile, SM, srcMgr);
	
	// Close out the file
	outfile.close();
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
	analyzeSILModule(SM);
}