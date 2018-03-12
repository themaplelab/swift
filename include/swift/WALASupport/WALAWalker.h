//===--- WALAWalker.h - frontend utility methods ----------------*- C++ -*-===//
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
#include <unordered_map>
#include <list>

#include "swift/SIL/SILModule.h"
#include "llvm/Support/FileSystem.h"
#include "swift/WALASupport/BasicBlockLabeller.h"
#include "swift/WALASupport/SymbolTable.h"


#include "Exceptions.h"
#include "CAstWrapper.h"

#ifndef SWIFT_WALAWALKER_H
#define SWIFT_WALAWALKER_H

//TODO: do this after silgen
//and possibly the "mandatory optimization pipeline"

using std::string;
using std::unordered_map;
using std::list;

namespace swift {

class WALAIntegration {
private:
  JNIEnv *java_env;
  Exceptions &cpp_ex;
  jobject xlator;
  CAstWrapper *CAst;

public:
  CAstWrapper *operator->();
  
  void print(jobject obj);
  
  jobject makePosition(int, int, int, int);

  jobject makeBigDecimal(const char *, int);
  
  WALAIntegration(JNIEnv *, Exceptions &, const char *);
};

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
    SILInstructionKind instrKind;
    
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
  bool printStdout = false;
//   llvm::raw_fd_ostream &outfile;
  
  // Gets the mangled and demangled SILFunction and returns in a FunctionInfo.
  WALAWalker::FunctionInfo getSILFunctionInfo(SILFunction &func);
  
  // Gets the sourcefile, start line/col, end line/col, and writes it to the 
  // InstrInfo that is passed in.
  void getInstrSrcInfo(SILInstruction &instr, WALAWalker::InstrInfo *instrInfo);
  
  // The big one - gets the ValueKind of the SILInstruction then goes       
  // through the mega-switch to cast and handle each appropriately.
  SILInstructionKind getInstrValueKindInfo(SILInstruction &instr, WALAIntegration &wala, 
                  unordered_map<void*, jobject>* nodeMap, list<jobject>* nodeList,
                  SymbolTable* symbolTable, BasicBlockLabeller* labeller);

  // Do something per instruction
  void perInstruction(WALAWalker::InstrInfo *instrInfo, WALAIntegration &);
    
public:
  void analyzeSILModule(SILModule &SM);
};
    
} // end namespace swift

#endif
