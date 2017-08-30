//===--- WALAWaler.h - frontend utility methods ------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file contains declarations of utility methods for parsing and
// performing semantic on modules.
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <stdio.h>
#include <string>

#include "swift/SIL/SILModule.h"
#include "llvm/Support/FileSystem.h"

#ifndef SWIFT_WALAWALKER_H
#define SWIFT_WALAWALKER_H

//TODO: do this after silgen
//and possibly the "mandatory optimization pipeline"

using std::string;

namespace swift {

class WALAWalker {

private:
	bool debug = true;
	bool printStdout = true;
	
	// Gets shortFilename from full filenamePath, concatenates it to
	// $SWIFT_WALA_OUTPUT dir, and writes the result to outfileName.
	void getOutputFilename(string filenamePath, char *outfileName);
	
	// Prints the path (after swift/) to outfile if it is open and writeable.
	void printSourceFilepath(llvm::raw_ostream &outfile, 
		SILModule &SM);
	
	// Prints the SIL to the outfile if it is open and writeable.
	void printSIL(char *outputFilename, SILModule &SM);
	
	// Outputs the mangled and demangled SILFunction name to outfile.
	void printSILFunctionInfo(llvm::raw_ostream &outfile, 
		SILFunction &func);
	
	// Outputs the SILBasicBlock ID to outstream.
	void printSILBasicBlockInfo(llvm::raw_ostream &outfile, 
		SILBasicBlock &bb);
	
	// Prints the sourcefile, line, and column info to outstream.
	void printInstrDebugLocInfo(llvm::raw_ostream &outfile,
		SILInstruction &instr, SourceManager &srcMgr);
		
	// Outputs whether instr may release, write to / read from memory,
	// trap the instruction, or potentially have side effects.
	void printInstrMemoryReleasingInfo(llvm::raw_ostream &outfile, 
		SILInstruction &instr);
	
	// Goes over all operands on the SILInstr and prints them out.
	void printInstrOpInfo(llvm::raw_ostream &outfile, 
		SILInstruction &instr);

	// The big one - gets the ValueKind of the SILInstruction then goes 			
	// through the mega-switch to cast and handle each appropriately.
	void printInstrValueKindInfo(llvm::raw_ostream &outfile, 
		SILInstruction &instr);
	
	// Handles all the SILInstruction printing and management.
	void printSILInstrInfo(llvm::raw_ostream &outfile,
		SILInstruction &instr, SourceManager &srcMgr);
    
    // Break down SILModule -> SILFunction -> SILBasicBlock -> SILInstruction
	void getModBreakdown(llvm::raw_ostream &outfile, SILModule &SM);
	
	// Debugging for why some SILModules don't return a source filename
	void sourcefileDebug(SILModule &SM);
    
public:
	void foo();
	void analyzeSILModule(SILModule &SM);
};
    
} // end namespace swift

#endif
