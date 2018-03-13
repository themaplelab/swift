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
    explicit ModuleInfo(StringRef sourcefile) : sourcefile(sourcefile) {};
    StringRef sourcefile;
  };

  struct FunctionInfo {
    FunctionInfo(StringRef name, StringRef demangled) : name(name), demangled(demangled) {};
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

public:
  void analyzeSILModule(SILModule &SM);
};
    
} // end namespace swift

#endif
