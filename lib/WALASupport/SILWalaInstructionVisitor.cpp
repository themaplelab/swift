#include "swift/WALASupport/SILWalaInstructionVisitor.h"
#include "swift/WALASupport/InstrKindInfoGetter.h"

#include <swift/Demangling/Demangle.h>

namespace swift {

// Gets the sourcefile, start line/col, end line/col, and writes it to the InstrInfo
// that is passed in.
// TODO: check lastBuffer vs. buffer to see if start and end are in the same file
void SILWalaInstructionVisitor::updateInstrSourceInfo(SILInstruction *I) {
  SourceManager &srcMgr = I->getModule().getSourceManager();

  // Get file-line-col information for the source
  SILLocation debugLoc = I->getDebugLocation().getLocation();
  SILLocation::DebugLoc debugInfo = debugLoc.decodeDebugLoc(srcMgr);

  InstrInfo->Filename = debugInfo.Filename;

  if (I->getLoc().isNull()) {
    InstrInfo->srcType = -1;
  } else {

    SourceRange srcRange = I->getLoc().getSourceRange();
    SourceLoc srcStart = srcRange.Start;
    SourceLoc srcEnd = srcRange.End;

    if (srcStart.isValid() ) {

      InstrInfo->srcType = 0;

      auto startLineCol = srcMgr.getLineAndColumn(srcStart);
      InstrInfo->startLine = startLineCol.first;
      InstrInfo->startCol = startLineCol.second;

    } else {
      InstrInfo->startLine = debugInfo.Line;
      InstrInfo->startCol = debugInfo.Column;
    }

    if (srcEnd.isValid() ) {

      auto endLineCol = srcMgr.getLineAndColumn(srcEnd);
      InstrInfo->endLine = endLineCol.first;
      InstrInfo->endCol = endLineCol.second;
    } else {
      InstrInfo->srcType = 1;
    }
  }
}

void SILWalaInstructionVisitor::beforeVisit(SILInstruction *I) {
  InstrInfo = std::make_shared<WALAWalker::InstrInfo>();

  updateInstrSourceInfo(I);

  InstrInfo->num = InstructionCount++;
  InstrInfo->memBehavior = I->getMemoryBehavior();
  InstrInfo->relBehavior = I->getReleasingBehavior();

  //FixMe: replace with weak pointer.
  InstrInfo->modInfo = ModuleInfo.get();
  InstrInfo->funcInfo = FunctionInfo.get();
  InstrInfo->instrKind = I->getKind();

  std::vector<SILValue> vals;
  for (const auto &op: I->getAllOperands()) {
    vals.push_back(op.get());
  }
  InstrInfo->ops = llvm::ArrayRef<SILValue>(vals);
}

void SILWalaInstructionVisitor::visitSILBasicBlock(SILBasicBlock *BB) {
  llvm::outs() << "Basic Block: " ;
  llvm::outs() << BB << "\n";
  llvm::outs() << "SILFunctions: " << BB->getParent() << "\n";
  InstructionCount = 0;
  NodeMap.clear();
  NodeList.clear();

  for (auto &I: *BB) {
    auto Node = visit(&I);
    if (Node != nullptr) {
      NodeList.push_back(Node);
    }
  }

  if (NodeList.size() > 0) {
    jobject labelNode = Wala->makeConstant(BasicBlockLabeller::label(BB).c_str());
    jobject labelStmt = Wala->makeNode(CAstWrapper::LABEL_STMT, labelNode, NodeList.front());
    NodeList.pop_front();
    NodeList.push_front(labelStmt);
    jobject blockStmt = Wala->makeNode(CAstWrapper::BLOCK_STMT, Wala->makeArray(&NodeList));
    BlockStmtList.push_back(blockStmt);
  }
}

void SILWalaInstructionVisitor::visitSILFunction(SILFunction *F) {
  FunctionInfo = std::make_shared<WALAWalker::FunctionInfo>(F->getName(), Demangle::demangleSymbolAsString(F->getName()));
  BlockStmtList.clear();
  llvm::outs() << "SILFunction: " ;
  llvm::outs() << F << "\n";
  F->print(llvm::outs(), true);

  for (auto &BB: *F) {
    visitSILBasicBlock(&BB);
  }

  for (auto &Stmt: BlockStmtList) {
    Wala.print(Stmt);
  }
}

void SILWalaInstructionVisitor::visitModule(SILModule *M) {
  ModuleInfo = std::make_shared<WALAWalker::ModuleInfo>(M->getSwiftModule()->getModuleFilename());
  if (ModuleInfo->sourcefile.empty()) {
    ModuleInfo->sourcefile = "N/A";
  }
  for (auto &F: *M) {
    visitSILFunction(&F);
  }
}

// Actions to take on a per-instruction basis.  InstrInfo contains all the relevant info
// for the current instruction in the iteration.
void SILWalaInstructionVisitor::perInstruction() {

  raw_ostream &outs = llvm::outs();

  Wala.print(Wala.makePosition(
      InstrInfo->startLine, InstrInfo->startCol,
      InstrInfo->endLine, InstrInfo->endCol)
  );

  if (Print) {
    outs << "\t [INSTR] #" << InstrInfo->num;
    outs << ", [OPNUM] " << InstrInfo->id << "\n";
    outs << "\t --> File: " << InstrInfo->Filename << "\n";

    // Has no location information
    if (InstrInfo->srcType == -1) {
      outs << "\t **** No source information. \n";

      // Has only start information
    } else {
      outs << "\t ++++ Start - Line " << InstrInfo->startLine << ":"
           << InstrInfo->startCol << "\n";
    }

    // Has end information
    if (InstrInfo->srcType == 0) {
      outs << "\t ---- End - Line " << InstrInfo->endLine;
      outs << ":" << InstrInfo->endCol << "\n";
    }

    // Memory Behavior
    switch (InstrInfo->memBehavior) {
    case SILInstruction::MemoryBehavior::None: {
      break;
    }
    case SILInstruction::MemoryBehavior::MayRead: {
      outs << "\t\t +++ [MEM-R]: May read from memory. \n";
      break;
    }
    case SILInstruction::MemoryBehavior::MayWrite: {
      outs << "\t\t +++ [MEM-W]: May write to memory. \n";
      break;
    }
    case SILInstruction::MemoryBehavior::MayReadWrite: {
      outs << "\t\t +++ [MEM-RW]: May read or write memory. \n";
      break;
    }
    case SILInstruction::MemoryBehavior::MayHaveSideEffects: {
      outs << "\t\t +++ [MEM-F]: May have side effects. \n";
    }
    }

    // Releasing Behavior
    switch (InstrInfo->relBehavior) {
    case SILInstruction::ReleasingBehavior::DoesNotRelease: {
      outs << "\t\t [REL]: Does not release memory. \n";
      break;
    }
    case SILInstruction::ReleasingBehavior::MayRelease: {
      outs << "\t\t [REL]: May release memory. \n";
      break;
    }
    }

    // Show operands, if they exist
    for (auto op = InstrInfo->ops.begin(); op != InstrInfo->ops.end(); ++op) {
      outs << "\t\t [OPER]: " << *op;
    }

    outs << "\n";
  }
}

jobject SILWalaInstructionVisitor::findAndRemoveCAstNode(void *Key) {
  jobject node = nullptr;
  if (SymbolTable.has(Key)) {
    // this is a variable
    jobject name = Wala->makeConstant(SymbolTable.get(Key).c_str());
    node = Wala->makeNode(CAstWrapper::VAR, name);
  } else if (NodeMap.find(Key) != NodeMap.end()) {
    // find
    node = NodeMap.at(Key);
    // remove
    auto it = std::find(NodeList.begin(), NodeList.end(), node);
    if (it != NodeList.end()) {
      NodeList.erase(it);
    }
  }
  return node;
}


jobject SILWalaInstructionVisitor::visitApplyInst(ApplyInst *AI) {
  if (Print) {
    llvm::outs() << "<< ApplyInst >>" << "\n";
  }

  jobject node = nullptr; // the CAst node to be created

  auto *Callee = AI->getReferencedFunction();

  if (!Callee) {
    return nullptr;
  }

  auto *FD = Callee->getLocation().getAsASTNode<FuncDecl>();

  if (FD && (FD->isUnaryOperator() || FD->isBinaryOperator())) {
    Identifier name = FD->getName();
    jobject operatorNode = InstrKindInfoGetter::getOperatorCAstType(name);
    if (operatorNode != nullptr) {
      if (Print) {
        llvm::outs() << "\t Built in operator\n";
        for (unsigned i = 0; i < AI->getNumArguments(); ++i) {
          SILValue v = AI->getArgument(i);
          llvm::outs() << "\t [ARG] #" << i << ": " << v;
          llvm::outs() << "\t [ADDR] #" << i << ": " << v.getOpaqueValue() << "\n";
        }
      }

      if (FD->isUnaryOperator()) {
        // unary operator
        jobject operand = nullptr;
        if (AI->getNumArguments() >= 2) {
          SILValue argument = AI->getArgument(AI->getNumArguments() - 2); // the second last one (last one is metatype)
          operand = findAndRemoveCAstNode(argument.getOpaqueValue());
        }
        node = Wala->makeNode(CAstWrapper::UNARY_EXPR, operatorNode, operand);
      } else {
        // binary operator
        jobject firstOperand = nullptr;
        jobject secondOperand = nullptr;

        if (AI->getNumArguments() >= 3) {
          SILValue argument = AI->getArgument(AI->getNumArguments() - 3);
          firstOperand = findAndRemoveCAstNode(argument);
        }

        if (AI->getNumArguments() >= 2) {
          SILValue argument = AI->getArgument(AI->getNumArguments() - 2);
          secondOperand = findAndRemoveCAstNode(argument);
        }

        node = Wala->makeNode(CAstWrapper::BINARY_EXPR, operatorNode, firstOperand, secondOperand);
      }
      NodeMap.insert(std::make_pair(AI, node)); // insert the node into the hash map
      return node;
    } // otherwise, fall through to the regular funcion call logic
  }

  if (Print) {
    llvm::outs() << "\t [CALLEE]: " << Demangle::demangleSymbolAsString(Callee->getName()) << "\n";
  }

  jobject funcExprNode = findAndRemoveCAstNode(Callee);
  list<jobject> params;

  for (unsigned i = 0; i < AI->getNumArguments(); ++i) {
    SILValue v = AI->getArgument(i);
    jobject child = findAndRemoveCAstNode(v.getOpaqueValue());
    if (child != nullptr) {
      params.push_back(child);
    }

    if (Print) {
      llvm::outs() << "\t [ARG] #" << i << ": " << v;
      llvm::outs() << "\t [ADDR] #" << i << ": " << v.getOpaqueValue() << "\n";
    }
  }

  node = Wala->makeNode(CAstWrapper::CALL, funcExprNode, Wala->makeArray(&params));
  NodeMap.insert(std::make_pair(AI, node)); // insert the node into the hash map
  return node;
}

}