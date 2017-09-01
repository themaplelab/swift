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

public:

	struct ModuleInfo {
		StringRef sourcefile;
	};

	struct FunctionInfo {
		StringRef name;
		StringRef demangled;
	};
	
	struct InstrInfo {
		unsigned num;
		SILPrintContext::ID id;
		ValueKind instrKind;
		
		SILInstruction::MemoryBehavior memBehavior;
		SILInstruction::ReleasingBehavior relBehavior;

		short srcType;
		string Filename;
		unsigned startLine;
		unsigned startCol;
		unsigned endLine;
		unsigned endCol;
	
		ArrayRef<SILValue> ops;
		WALAWalker::ModuleInfo *modInfo;
		WALAWalker::FunctionInfo *funcInfo;
	};

private:
	bool printStdout = true;
// 	llvm::raw_fd_ostream &outfile;
	
	// Gets the mangled and demangled SILFunction and returns in a FunctionInfo.
	WALAWalker::FunctionInfo getSILFunctionInfo(SILFunction &func);
	
	// Gets the sourcefile, start line/col, end line/col, and writes it to the 
	// InstrInfo that is passed in.
	void getInstrSrcInfo(SILInstruction &instr, WALAWalker::InstrInfo *instrInfo);
	
	// The big one - gets the ValueKind of the SILInstruction then goes 			
	// through the mega-switch to cast and handle each appropriately.
	ValueKind getInstrValueKindInfo(SILInstruction &instr);

	// Do something per instruction
	void perInstruction(WALAWalker::InstrInfo *instrInfo);
    
public:
	void foo();
	void analyzeSILModule(SILModule &SM);
};
    
} // end namespace swift

#endif
