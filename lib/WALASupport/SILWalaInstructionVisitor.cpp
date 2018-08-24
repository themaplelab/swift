#include "swift/WALASupport/SILWalaInstructionVisitor.h"

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

    if (srcStart.isValid()) {

      InstrInfo->srcType = 0;

      auto startLineCol = srcMgr.getLineAndColumn(srcStart);
      InstrInfo->startLine = startLineCol.first;
      InstrInfo->startCol = startLineCol.second;

    } else {
      InstrInfo->startLine = debugInfo.Line;
      InstrInfo->startCol = debugInfo.Column;
    }

    if (srcEnd.isValid()) {

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
  perInstruction();

  if (Print) {
    llvm::outs() << "<< " << getSILInstructionName(I->getKind()) << " >>\n";
  }
}

void SILWalaInstructionVisitor::visitSILBasicBlock(SILBasicBlock *BB) {
  llvm::outs() << "Basic Block: ";
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
    jobject Node = Wala->makeConstant(BasicBlockLabeller::label(BB).c_str());
    jobject Stmt = Wala->makeNode(CAstWrapper::LABEL_STMT, Node, NodeList.front());
    NodeList.pop_front();
    NodeList.push_front(Stmt);
    jobject BlockStmt = Wala->makeNode(CAstWrapper::BLOCK_STMT, Wala->makeArray(&NodeList));
    BlockStmtList.push_back(BlockStmt);
  }
}

void SILWalaInstructionVisitor::visitSILFunction(SILFunction *F) {
  FunctionInfo =
      std::make_shared<WALAWalker::FunctionInfo>(F->getName(), Demangle::demangleSymbolAsString(F->getName()));
  BlockStmtList.clear();
  llvm::outs() << "SILFunction: ";
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
  Wala.print(Wala.makePosition(
      InstrInfo->startLine, InstrInfo->startCol,
      InstrInfo->endLine, InstrInfo->endCol)
  );

  if (Print) {
    llvm::outs() << "\t [INSTR] #" << InstrInfo->num;
    llvm::outs() << ", [OPNUM] " << InstrInfo->id << "\n";
    llvm::outs() << "\t --> File: " << InstrInfo->Filename << "\n";

    // Has no location information
    if (InstrInfo->srcType == -1) {
      llvm::outs() << "\t **** No source information. \n";

      // Has only start information
    } else {
      llvm::outs() << "\t ++++ Start - Line " << InstrInfo->startLine << ":"
                   << InstrInfo->startCol << "\n";
    }

    // Has end information
    if (InstrInfo->srcType == 0) {
      llvm::outs() << "\t ---- End - Line " << InstrInfo->endLine;
      llvm::outs() << ":" << InstrInfo->endCol << "\n";
    }

    // Memory Behavior
    switch (InstrInfo->memBehavior) {
    case SILInstruction::MemoryBehavior::None: {
      break;
    }
    case SILInstruction::MemoryBehavior::MayRead: {
      llvm::outs() << "\t\t +++ [MEM-R]: May read from memory. \n";
      break;
    }
    case SILInstruction::MemoryBehavior::MayWrite: {
      llvm::outs() << "\t\t +++ [MEM-W]: May write to memory. \n";
      break;
    }
    case SILInstruction::MemoryBehavior::MayReadWrite: {
      llvm::outs() << "\t\t +++ [MEM-RW]: May read or write memory. \n";
      break;
    }
    case SILInstruction::MemoryBehavior::MayHaveSideEffects: {
      llvm::outs() << "\t\t +++ [MEM-F]: May have side effects. \n";
    }
    }

    // Releasing Behavior
    switch (InstrInfo->relBehavior) {
    case SILInstruction::ReleasingBehavior::DoesNotRelease: {
      llvm::outs() << "\t\t [REL]: Does not release memory. \n";
      break;
    }
    case SILInstruction::ReleasingBehavior::MayRelease: {
      llvm::outs() << "\t\t [REL]: May release memory. \n";
      break;
    }
    }

    // Show operands, if they exist
    for (auto op = InstrInfo->ops.begin(); op != InstrInfo->ops.end(); ++op) {
      llvm::outs() << "\t\t [OPER]: " << *op;
    }

    llvm::outs() << "\n";
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

jobject SILWalaInstructionVisitor::getOperatorCAstType(Identifier Name) {
  if (Name.is("==")) {
    return CAstWrapper::OP_EQ;
  } else if (Name.is("!=")) {
    return CAstWrapper::OP_NE;
  } else if (Name.is("+")) {
    return CAstWrapper::OP_ADD;
  } else if (Name.is("/")) {
    return CAstWrapper::OP_DIV;
  } else if (Name.is("<<")) {
    return CAstWrapper::OP_LSH;
  } else if (Name.is("*")) {
    return CAstWrapper::OP_MUL;
  } else if (Name.is(">>")) {
    return CAstWrapper::OP_RSH;
  } else if (Name.is("-")) {
    return CAstWrapper::OP_SUB;
  } else if (Name.is(">=")) {
    return CAstWrapper::OP_GE;
  } else if (Name.is(">")) {
    return CAstWrapper::OP_GT;
  } else if (Name.is("<=")) {
    return CAstWrapper::OP_LE;
  } else if (Name.is("<")) {
    return CAstWrapper::OP_LT;
  } else if (Name.is("!")) {
    return CAstWrapper::OP_NOT;
  } else if (Name.is("~")) {
    return CAstWrapper::OP_BITNOT;
  } else if (Name.is("&")) {
    return CAstWrapper::OP_BIT_AND;
  } else if (Name.is("&&")) {
    // return CAstWrapper::OP_REL_AND;
    return nullptr; // && and || are handled separatedly because they involve short circuits
  } else if (Name.is("|")) {
    return CAstWrapper::OP_BIT_OR;
  } else if (Name.is("||")) {
    // return CAstWrapper::OP_REL_OR;
    return nullptr; // && and || are handled separatedly because they involve short circuits
  } else if (Name.is("^")) {
    return CAstWrapper::OP_BIT_XOR;
  } else {
    return nullptr;
  }
}

/*******************************************************************************/
/*                         ALLOCATION AND DEALLOCATION                         */
/*******************************************************************************/

jobject SILWalaInstructionVisitor::visitAllocStackInst(AllocStackInst *ASI) {
  SILDebugVariable Info = ASI->getVarInfo();
  unsigned ArgNo = Info.ArgNo;

  if (auto *Decl = ASI->getDecl()) {
    StringRef varName = Decl->getNameStr();
    if (Print) {
      llvm::outs() << "\t [ARG]#" << ArgNo << ": " << varName << "\n";
    }
    SymbolTable.insert(static_cast<ValueBase *>(ASI), varName);
  }
  else {
    // temporary allocation when referencing self.
    if (Print) {
      llvm::outs() << "\t [ARG]#" << ArgNo << ": " << "self" << "\n";
    }
    SymbolTable.insert(static_cast<ValueBase *>(ASI), "self");
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitAllocBoxInst(AllocBoxInst *ABI) {
  SILDebugVariable Info = ABI->getVarInfo();
  unsigned ArgNo = Info.ArgNo;

  if (auto *Decl = ABI->getDecl()) {
    StringRef varName = Decl->getNameStr();
    if (Print) {
      llvm::outs() << "\t [ARG]#" << ArgNo << ": " << varName << "\n";
    }
    SymbolTable.insert(static_cast<ValueBase *>(ABI), varName);
  }
  else {
    // temporary allocation when referencing self.
    if (Print) {
      llvm::outs() << "\t [ARG]#" << ArgNo << ": " << "self" << "\n";
    }
    SymbolTable.insert(static_cast<ValueBase *>(ABI), "self");
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitAllocRefInst(AllocRefInst *ARI) {
  string RefTypeName = ARI->getType().getAsString();

  if (Print) {
    llvm::outs() << "\t [VAR TYPE]: " << RefTypeName << "\n";
  }

  SymbolTable.insert(static_cast<ValueBase *>(ARI), RefTypeName);

  ArrayRef<SILType> Types = ARI->getTailAllocatedTypes();
  ArrayRef<Operand> Counts = ARI->getTailAllocatedCounts();

  for (unsigned Idx = 0, NumTypes = Types.size(); Idx < NumTypes; ++Idx) {
    SILValue OperandValue = Counts[Idx].get();
    string OperandTypeName = Types[Idx].getAsString();

    if (Print) {
      llvm::outs() << "\t [OPERAND]: " << OperandValue.getOpaqueValue() << " [TYPE]: " << OperandTypeName << "\n";
    }

    SymbolTable.insert(static_cast<ValueBase *>(OperandValue.getOpaqueValue()), OperandTypeName);
  }
  return  Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitAllocGlobalInst(AllocGlobalInst *AGI) {
  SILGlobalVariable *Var = AGI->getReferencedGlobal();
  StringRef Name = Var->getName();
  SILType Type = Var->getLoweredType();
  if (Print) {
    llvm::outs() << "\t [VAR NAME]:" << Name << "\n";
    llvm::outs() << "\t [VAR TYPE]:" << Type << "\n";
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitDeallocStackInst(DeallocStackInst *DSI) {
  if (Print) {
    for (auto &OP : DSI->getAllOperands()) {
      llvm::outs() << "\t [OPERAND]: " << OP.get() << "\n";
      llvm::outs() << "\t [ADDR]: " << OP.get().getOpaqueValue() << "\n";
    }
  }
  return  Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitDeallocBoxInst(DeallocBoxInst *DBI) {
  for (auto &OP : DBI->getAllOperands()) {
    if (Print) {
      llvm::outs() << "\t [OPERAND]: " << OP.get() << "\n";
      llvm::outs() << "\t [BOX]: " << OP.get().getOpaqueValue() << "\n";
    }
    SymbolTable.remove(OP.get().getOpaqueValue());
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitDeallocRefInst(DeallocRefInst *DRI) {
    for (auto &OP : DRI->getAllOperands()) {
    if (Print) {
      llvm::outs() << "\t [OPERAND]: " << OP.get() << "\n";
      llvm::outs() << "\t [REF]: " << OP.get().getOpaqueValue() << "\n";
    }
    SymbolTable.remove(OP.get().getOpaqueValue());
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitProjectBoxInst(ProjectBoxInst *PBI) {
  if (SymbolTable.has(PBI->getOperand().getOpaqueValue())) {
    // This is a variable
    // NOTE: Apple documentation states: This instruction has undefined behavior if the box is not currently allocated
    //       (link: https://github.com/apple/swift/blob/master/docs/SIL.rst#project_box) so there is no need to allocate
    //       it if it is not currently in the Symbol Table
    SymbolTable.duplicate(static_cast<ValueBase *>(PBI), SymbolTable.get(PBI->getOperand().getOpaqueValue()).c_str());
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitAllocValueBufferInst(AllocValueBufferInst *AVBI) {
  SILValue ValueBuffer = AVBI->getOperand();
  string ValueTypeName = AVBI->getValueType().getAsString();

  if (Print) {
    llvm::outs() << "\t [VALUE BUFFER]: " << ValueBuffer.getOpaqueValue() << "\n";
    llvm::outs() << "\t [VALUE TYPE]: " << ValueTypeName << "\n";
  }

  SymbolTable.insert(static_cast<ValueBase *>(ValueBuffer.getOpaqueValue()), ValueTypeName);
  return  Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitDeallocValueBufferInst(DeallocValueBufferInst *DVBI) {
  SILValue BufferValue = DVBI->getOperand();
  string ValueTypeName = DVBI->getValueType().getAsString();

  if (Print) {
    llvm::outs() << "\t [BUFFER ADDR]: " << BufferValue.getOpaqueValue() << "\n";
    llvm::outs() << "\t [VALUE TYPE]: " << ValueTypeName << "\n";
  }

  SymbolTable.remove(BufferValue.getOpaqueValue());
  return  Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitProjectValueBufferInst(ProjectValueBufferInst *PVBI) {
  SILValue BufferValue = PVBI->getOperand();
  string ValueTypeName = PVBI->getValueType().getAsString();

  if (Print) {
    llvm::outs() << "\t [BUFFER ADDR]: " << BufferValue.getOpaqueValue() << "\n";
    llvm::outs() << "\t [VALUE TYPE]: " << ValueTypeName << "\n";
  }

  // NOTE: Apple documentation states: This instruction has undefined behavior if the value buffer is not currently allocated
  //       (link: https://github.com/apple/swift/blob/master/docs/SIL.rst#project-value-buffer) so there is no need to allocate
  //       it if it is not currently in the Symbol Table
  if (SymbolTable.has(BufferValue.getOpaqueValue())) {
    SymbolTable.duplicate(static_cast<ValueBase *>(PVBI), SymbolTable.get(BufferValue.getOpaqueValue()).c_str());
  }

  return Wala->makeNode(CAstWrapper::EMPTY);
}

/*******************************************************************************/
/*                        DEBUG INFROMATION                                    */
/*******************************************************************************/

jobject SILWalaInstructionVisitor::visitDebugValueInst(DebugValueInst *DBI) {
  SILDebugVariable Info = DBI->getVarInfo();
  unsigned ArgNo = Info.ArgNo;

  if (Print) {
    llvm::outs() << "[ARGNO]: " << ArgNo << "\n";
  }

  VarDecl *Decl = DBI->getDecl();

  if (Decl) {
    string VarName = Decl->getNameStr();
    if (VarName.length() == 0) {
      llvm::outs() << "\t DebugValue empty name \n";
      return Wala->makeNode(CAstWrapper::EMPTY);
    }
    SILValue Val = DBI->getOperand();
    if (!Val) {
      if (Print) {
        llvm::outs() << "\t Operand is null\n";
      }
      return Wala->makeNode(CAstWrapper::EMPTY);
    }

    void *Addr = Val.getOpaqueValue();
    if (Addr) {
      SymbolTable.insert(Addr, VarName);
      llvm::outs() << "\t [ADDR OF OPERAND]:" << Addr << "\n";
    }
    else {
      if (Print) {
        llvm::outs() << "\t Operand OpaqueValue is null\n";
      }
      return Wala->makeNode(CAstWrapper::EMPTY);
    }
  }
  else {
    if (Print) {
      llvm::outs() << "\t Decl not found\n";
    }
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitDebugValueAddrInst(DebugValueAddrInst *DVAI) {

  SILDebugVariable DebugVar = DVAI->getVarInfo();

  if (Print) {
    llvm::outs() << "\t [ARGNO]: " << DebugVar.ArgNo << "\n";
  }

  VarDecl *Decl = DVAI->getDecl();

  if (Decl) {
    string VarName = Decl->getNameStr();
    if (Print) {
      llvm::outs() << "\t [DECL NAME]: " << VarName << "\n";
    }

    SILValue Operand = DVAI->getOperand();
    if (Operand) {
      void *Addr = Operand.getOpaqueValue();
      if (Print) {
        llvm::outs() << "\t [ADDR OF OPERAND]: " << Addr << "\n";
      }

      SymbolTable.insert(Addr, VarName);

    } else {
      if (Print) {
        llvm::outs() << "\t OPERAND IS NULL" << "\n";
      }
      return Wala->makeNode(CAstWrapper::EMPTY);;
    }

  } else {
      if (Print) {
        llvm::outs() << "\t DECL IS NULL" << "\n";
      }
    return Wala->makeNode(CAstWrapper::EMPTY);
  }

  return Wala->makeNode(CAstWrapper::EMPTY);
}

/*******************************************************************************/
/*                                ACCESSING MEMORY                             */
/*******************************************************************************/

jobject SILWalaInstructionVisitor::visitLoadInst(LoadInst *LI) {
  SILValue LoadOperand = LI->getOperand();

  if (Print) {
    llvm::outs() << "\t [NAME]:" << LoadOperand.getOpaqueValue() << "\n";
  }

  jobject Node = findAndRemoveCAstNode(LoadOperand.getOpaqueValue());

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(LI), Node));

  return Node;
}

jobject SILWalaInstructionVisitor::visitStoreInst(StoreInst *SI) {
  // Cast the instr to access methods
  SILValue Src = SI->getSrc();
  SILValue Dest = SI->getDest();
  if (Print) {
    llvm::outs() << "\t [SRC ADDR]: " << Src.getOpaqueValue() << "\n";
    llvm::outs() << "\t [DEST ADDR]: " << Dest.getOpaqueValue() << "\n";
  }

  jobject Node = Wala->makeNode(CAstWrapper::EMPTY);
  if (SymbolTable.has(Dest.getOpaqueValue())) {
    jobject Var = findAndRemoveCAstNode(Dest.getOpaqueValue());
    Node = Wala->makeNode(CAstWrapper::ASSIGN, Var, findAndRemoveCAstNode(Src.getOpaqueValue()));
  }

  // sometimes SIL creates temporary memory on the stack
  // the following code represents the correspondence between the origial value and the new temporary location
  if (NodeMap.find(Src.getOpaqueValue()) != NodeMap.end()) {
    NodeMap.insert(std::make_pair(Dest.getOpaqueValue(), NodeMap.at(Src.getOpaqueValue())));
  }
  return Node;
}

jobject SILWalaInstructionVisitor::visitBeginBorrowInst(BeginBorrowInst *BBI) {
  if (Print) {
    llvm::outs() << "\t [BBI]:" << BBI << "\n";
    llvm::outs() << "\t [OPERAND]:" << BBI->getOperand() << "\n";
    llvm::outs() << "\t [OPERAND ADDR]:" << BBI->getOperand().getOpaqueValue() << "\n";
  }
  jobject Node = findAndRemoveCAstNode(BBI->getOperand().getOpaqueValue());
  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(BBI), Node));
  return Node;
}

jobject SILWalaInstructionVisitor::visitLoadBorrowInst(LoadBorrowInst *LBI) {
  if (Print) {
    llvm::outs() << "\t [NAME]: " << LBI->getOperand() << "\n";
    llvm::outs() << "\t [ADDR]: " << LBI->getOperand().getOpaqueValue() << "\n";
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitEndBorrowInst(EndBorrowInst *EBI) {
  if (Print) {
    llvm::outs() << "\t [BORROWED VALUE]: " << EBI->getBorrowedValue() << "\n";
    llvm::outs() << "\t [BORROWED VALUE ADDR]: " << EBI->getBorrowedValue().getOpaqueValue() << "\n";
    llvm::outs() << "\t [ORRIGINAL VALUE]: " << EBI->getOriginalValue() << "\n";
    llvm::outs() << "\t [ORRIGINAL VALUE ADDR]: " << EBI->getOriginalValue().getOpaqueValue() << "\n";
  }
  if (NodeMap.find(EBI->getBorrowedValue()) != NodeMap.end()) {
    if (Print) {
      llvm::outs() << "\t borrowed value found in NodeMap, remove from NodeMap\n";
    }
    NodeMap.erase(EBI->getBorrowedValue());
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}


jobject SILWalaInstructionVisitor::visitAssignInst(AssignInst *AI) {

  if (Print) {
    llvm::outs() << "\t [SOURCE]: " << AI->getSrc().getOpaqueValue() << "\n";
    llvm::outs() << "\t [DEST]: " << AI->getDest().getOpaqueValue() << "\n";
  }
  jobject Dest = findAndRemoveCAstNode(AI->getDest().getOpaqueValue());
  jobject Expr = findAndRemoveCAstNode(AI->getSrc().getOpaqueValue());

  jobject Node = Wala->makeNode(CAstWrapper::ASSIGN, Dest, Expr);
  NodeMap.insert(std::make_pair(AI, Node));
  return Node;
}

jobject SILWalaInstructionVisitor::visitStoreBorrowInst(StoreBorrowInst *SBI) {
  SILValue SourceBorrowed = SBI->getSrc();
  SILValue DestBorrowed = SBI->getDest();

  if (Print) {
    llvm::outs() << "\t [SOURCE BORROWED ADDR]: " << SourceBorrowed.getOpaqueValue() << "\n";
    llvm::outs() << "\t [DEST BORROWED ADDR]: " << DestBorrowed.getOpaqueValue() << "\n";
  }

  jobject Dest = findAndRemoveCAstNode(SourceBorrowed.getOpaqueValue());
  jobject Source = findAndRemoveCAstNode(DestBorrowed.getOpaqueValue());

  jobject Node = Wala->makeNode(CAstWrapper::ASSIGN, Dest, Source);
  NodeMap.insert(std::make_pair(SBI, Node));
  return Node;
}

jobject SILWalaInstructionVisitor::visitMarkUninitializedInst(MarkUninitializedInst *MUI) {
  // This instruction just marks the uninitialized memory locations
  // So from the perspective of Wala no operations is going on here
  // We would just return an empty node here
  SILValue AddressToBeMarked = MUI->getOperand();
  string KindOfMark("");
  switch(MUI->getKind()){
    case 0: { KindOfMark = "Var"; } break;
    case 1: { KindOfMark = "RootSelf"; } break;
    case 2: { KindOfMark = "CrossModuleRootSelf"; } break;
    case 3: { KindOfMark = "DerivedSelf"; } break;
    case 4: { KindOfMark = "DerivedSelfOnly"; } break;
    case 5: { KindOfMark = "DelegatingSelf"; } break;
  }
  if (Print) {
    llvm::outs() << "\t [MARK]: " << AddressToBeMarked.getOpaqueValue() << " AS " << KindOfMark << "\n";
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitMarkFunctionEscapeInst(MarkFunctionEscapeInst *MFEI){
  for(Operand &MFEOperand : MFEI->getAllOperands()){
    unsigned OperandNumber = MFEOperand.getOperandNumber();
    SILValue OperandValue = MFEOperand.get();
    if (Print) {
      llvm::outs() << "\t [OPERAND NO]: " << OperandNumber << " [VALUE]: " << OperandValue.getOpaqueValue() << "\n";
    }
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitCopyAddrInst(CopyAddrInst *CAI) {

  SILValue Source = CAI->getSrc();
  SILValue Dest = CAI->getDest();

  if (Print) {
    llvm::outs() << "\t [SOURCE ADDR]: " << Source.getOpaqueValue() << "\n";
    llvm::outs() << "\t [DEST ADDR]: " << Dest.getOpaqueValue() << "\n";
  }

  jobject NewVar = findAndRemoveCAstNode(Source.getOpaqueValue());
  jobject OldVar = findAndRemoveCAstNode(Dest.getOpaqueValue());
  
  jobject Node = Wala->makeNode(CAstWrapper::ASSIGN, NewVar, OldVar);
  NodeMap.insert(std::make_pair(CAI, Node));

  return Node;
}

jobject SILWalaInstructionVisitor::visitDestroyAddrInst(DestroyAddrInst *DAI) {

    SILValue DestroyAddr = DAI->getOperand();

  if (Print) {
      llvm::outs() << "\t [ADDR TO DESTROY]: " << DestroyAddr.getOpaqueValue() << "\n";
  }

  findAndRemoveCAstNode(DestroyAddr.getOpaqueValue());
  SymbolTable.remove(DestroyAddr.getOpaqueValue());

  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitIndexAddrInst(IndexAddrInst *IAI) {
  SILValue base = IAI->getBase();
  SILValue idx = IAI->getIndex();

  if(Print){
    llvm::outs() << "\t [BASE ADDR]" << base.getOpaqueValue() << "\n";
    llvm::outs() << "\t [INDEX ADDR]" << idx.getOpaqueValue() << "\n";
  }

  jobject BaseNode = findAndRemoveCAstNode(base.getOpaqueValue());
  jobject IndexNode = findAndRemoveCAstNode(idx.getOpaqueValue());

  jobject Node = Wala->makeNode(CAstWrapper::EMPTY);
  Node = Wala->makeNode(CAstWrapper::ARRAY_REF, BaseNode , IndexNode);

  if(Node != nullptr) {
    NodeMap.insert(std::make_pair(static_cast<ValueBase *>(IAI), Node));
    return Node;
  }
  
  return Wala->makeNode(CAstWrapper::EMPTY); 
}

jobject SILWalaInstructionVisitor::visitTailAddrInst(TailAddrInst *TAI) {
  SILValue BaseVale = TAI->getBase();
  SILValue IndexValue = TAI->getIndex();
  SILType  ResultType = TAI->getTailType();

  if(Print){
    llvm::outs() << "\t [BASE ADDR]" << BaseVale.getOpaqueValue() << "\n";
    llvm::outs() << "\t [INDEX ADDR]" << IndexValue.getOpaqueValue() << "\n";
    llvm::outs() << "\t [RESULT TYPE]" << ResultType.getAsString() << "\n";
  }

  jobject BaseNode = findAndRemoveCAstNode(BaseVale.getOpaqueValue());
  jobject IndexNode = findAndRemoveCAstNode(IndexValue.getOpaqueValue());

  jobject Node = Wala->makeNode(CAstWrapper::ARRAY_REF, BaseNode, IndexNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(TAI), Node));
  return Node;
}

jobject SILWalaInstructionVisitor::visitBeginAccessInst(BeginAccessInst *BAI) {
  if (Print) {
    llvm::outs() << "\t [OPERAND ADDR]:" << (BAI->getSource()).getOpaqueValue() << "\n";
  }
  jobject Var = findAndRemoveCAstNode(BAI->getSource().getOpaqueValue());
  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(BAI), Var));
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitEndAccessInst(EndAccessInst *EAI) {
  if (Print) {
    llvm::outs() << "\t [BEGIN ACCESS]: " << EAI->getBeginAccess() << "\n";
  }
  ValueBase *key = static_cast<ValueBase *>(EAI->getBeginAccess());
  if (NodeMap.find(key) != NodeMap.end()) {
    if (Print) {
      llvm::outs() << "\t borrowed value found in NodeMap, remove from NodeMap\n";
    }
    NodeMap.erase(key);
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitBeginUnpairedAccessInst(BeginUnpairedAccessInst *BUI) {

  SILValue SourceValue = BUI->getSource();
  SILValue BufferValue = BUI->getBuffer();

  if (Print) {
    llvm::outs() << "\t [OPERAND]: " << SourceValue.getOpaqueValue() << "\n";
    llvm::outs() << "\t [BUFFER]: " << SourceValue.getOpaqueValue() << "\n";
  }

  jobject SourceNode = findAndRemoveCAstNode(SourceValue.getOpaqueValue());

  NodeMap.insert(std::make_pair(BufferValue.getOpaqueValue(), SourceNode));

  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitEndUnpairedAccessInst(EndUnpairedAccessInst *EUAI) {
  SILValue BufferValue = EUAI->getBuffer();

  if (Print) {
    llvm::outs() << "\t [BUFFER]: " << BufferValue << "\n";
    llvm::outs() << "\t [BUFFER ADDR]: " << BufferValue.getOpaqueValue() << "\n";
  }

  findAndRemoveCAstNode(BufferValue.getOpaqueValue());

  return Wala->makeNode(CAstWrapper::EMPTY);
}

/*******************************************************************************/
/*                              REFERENCE COUNTING                             */
/*******************************************************************************/

jobject SILWalaInstructionVisitor::visitMarkDependenceInst(MarkDependenceInst *MDI) {

  SILValue DependentValue = MDI->getValue();
  SILValue BaseValue = MDI->getBase();

  if (Print) {
    llvm::outs() << "\t [MarkDependence]: " << static_cast<ValueBase *>(MDI) << "\n";
    llvm::outs() << "\t [VALUE]: " << DependentValue.getOpaqueValue() << "\n";
    llvm::outs() << "\t validity depends on" << "\n";
    llvm::outs() << "\t [BASE]: " << BaseValue.getOpaqueValue() << "\n";
  }

  jobject DependentNode = findAndRemoveCAstNode(DependentValue.getOpaqueValue());
  jobject BaseNode = findAndRemoveCAstNode(BaseValue.getOpaqueValue());

  jobject Node = Wala->makeNode(CAstWrapper::PRIMITIVE, BaseNode, DependentNode );

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(MDI), Node));

  return Node;
}

/*******************************************************************************/
/*                                  LITERALS                                   */
/*******************************************************************************/

jobject SILWalaInstructionVisitor::visitFunctionRefInst(FunctionRefInst *FRI) {
  // Cast the instr to access methods
  string FuncName = Demangle::demangleSymbolAsString(FRI->getReferencedFunction()->getName());
  jobject NameNode = Wala->makeConstant(FuncName.c_str());
  jobject FuncExprNode = Wala->makeNode(CAstWrapper::FUNCTION_EXPR, NameNode);

  if (Print) {
    llvm::outs() << "\t [FUNCTION]: " << FuncName << "\n";
  }

  NodeMap.insert(std::make_pair(FRI->getReferencedFunction(), FuncExprNode));
  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(FRI), FuncExprNode));
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitGlobalAddrInst(GlobalAddrInst *GAI) {
  SILGlobalVariable *variable = GAI->getReferencedGlobal();
  StringRef Name = variable->getName();
  if (Print) {
    llvm::outs() << "\t [VAR NAME]:" << Name << "\n";
  }
  SymbolTable.insert(static_cast<ValueBase *>(GAI), Name);
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitIntegerLiteralInst(IntegerLiteralInst *ILI) {
  APInt Value = ILI->getValue();
  jobject Node = Wala->makeNode(CAstWrapper::EMPTY);
  if (Value.isNegative()) {
    if (Value.getMinSignedBits() <= 32) {
      Node = Wala->makeConstant(static_cast<int>(Value.getSExtValue()));
    } else if (Value.getMinSignedBits() <= 64) {
      Node = Wala->makeConstant(static_cast<long>(Value.getSExtValue()));
    }
  } else {
    if (Value.getActiveBits() <= 32) {
      Node = Wala->makeConstant(static_cast<int>(Value.getZExtValue()));
    } else if (Value.getActiveBits() <= 64) {
      Node = Wala->makeConstant(static_cast<long>(Value.getZExtValue()));
    }
  }
  if (Node != nullptr) {
    NodeMap.insert(std::make_pair(static_cast<ValueBase *>(ILI), Node));
  }
  return Node;
}


jobject SILWalaInstructionVisitor::visitFloatLiteralInst(FloatLiteralInst *FLI) {
  jobject Node = Wala->makeNode(CAstWrapper::EMPTY);
  APFloat Value = FLI->getValue();

  if (&Value.getSemantics() == &APFloat::IEEEsingle()) {
    // To Float
    Node = Wala->makeConstant(Value.convertToFloat());
  }
  else if (&Value.getSemantics() == &APFloat::IEEEdouble()) {
    // To Double
    Node = Wala->makeConstant(Value.convertToDouble());
  }
  else if (Value.isFinite()) {
    // To BigDecimal
    SmallVector<char, 128> buf;
    Value.toString(buf);
    jobject BigDecimal = Wala.makeBigDecimal(buf.data(), buf.size());
    Node = Wala->makeConstant(BigDecimal);
  }
  else {
    // Infinity or NaN, convert to double
    // as BigDecimal constructor cannot accept strings of these
    bool APFLosesInfo;
    Value.convert(APFloat::IEEEdouble(), APFloat::rmNearestTiesToEven, &APFLosesInfo);
    Node = Wala->makeConstant(Value.convertToDouble());
  }
  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(FLI), Node));
  return Node;
}

jobject SILWalaInstructionVisitor::visitConstStringLiteralInst(ConstStringLiteralInst *CSLI) {
  // Value: the string data for the literal, in UTF-8.
  StringRef Value = CSLI->getValue();
  if (Print) {
    llvm::outs() << "\t [VALUE] " << Value << "\n";
  }

  // Encoding: the desired encoding of the text.
  string E;
  switch (CSLI->getEncoding()) {
  case ConstStringLiteralInst::Encoding::UTF8: {
    E = "UTF8";
    break;
  }
  case ConstStringLiteralInst::Encoding::UTF16: {
    E = "UTF16";
    break;
  }
  }
  if (Print) {
    llvm::outs() << "\t [ENCODING]: " << E << "\n";
  }

  // Count: encoding-based length of the string literal in code units.
  uint64_t Count = CSLI->getCodeUnitCount();
  if (Print) {
    llvm::outs() << "\t [COUNT]: " << Count << "\n";
  }

  // Call WALA in Java
  jobject C = Wala->makeConstant((Value.str()).c_str());

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(CSLI), C));

  return C;
}

jobject SILWalaInstructionVisitor::visitStringLiteralInst(StringLiteralInst *SLI) {
  // Value: the string data for the literal, in UTF-8.
  StringRef Value = SLI->getValue();

  if (Print) {
    llvm::outs() << "\t [VALUE]: " << Value << "\n";
  }

  // Encoding: the desired encoding of the text.
  string encoding;
  switch (SLI->getEncoding()) {
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

  if (Print) {
    llvm::outs() << "\t [ENCODING]: " << encoding << "\n";
  }

  // Count: encoding-based length of the string literal in code units.
  uint64_t codeUnitCount = SLI->getCodeUnitCount();

  if (Print) {
    llvm::outs() << "\t [CODEUNITCOUNT]: " << codeUnitCount << "\n";
  }

  // Call WALA in Java
  jobject walaConstant = Wala->makeConstant((Value.str()).c_str());
  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(SLI), walaConstant));
  return walaConstant;
}

/*******************************************************************************/
/*                               DYNAMIC DISPATCH                              */
/*******************************************************************************/

jobject SILWalaInstructionVisitor::visitClassMethodInst(ClassMethodInst *CMI) {
  SILValue ClassOperand = CMI->getOperand();
  SILDeclRef MemberFunc = CMI->getMember();

  string MemberFuncName = Demangle::demangleSymbolAsString(MemberFunc.mangle());

  if (Print) {
    llvm::outs() << "\t [CLASS]: " << CMI->getMember().getDecl()->getInterfaceType().getString() << "\n";
    llvm::outs() << "\t [MEMBER]: " << MemberFuncName << "\n";
  }

  jobject ClassNode = findAndRemoveCAstNode(ClassOperand.getOpaqueValue());

  jobject MemberNameNode = Wala->makeConstant(MemberFuncName.c_str());  
  jobject FuncNode = Wala->makeNode(CAstWrapper::FUNCTION_EXPR, MemberNameNode);

  jobject Node = Wala->makeNode(CAstWrapper::OBJECT_REF, ClassNode, FuncNode );

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(CMI), Node));

  return Node;
}

jobject SILWalaInstructionVisitor::visitWitnessMethodInst(WitnessMethodInst *WMI) {

  ProtocolDecl *Protocol = WMI->getLookupProtocol();
  SILDeclRef MemberFunc = WMI->getMember();

  string ProtocolName = Protocol->getNameStr();
  string MemberFuncName = Demangle::demangleSymbolAsString(MemberFunc.mangle());

  if (Print) {
    llvm::outs() << "\t [PROTOCOL]: " << ProtocolName << "\n";
    llvm::outs() << "\t [MEMBER]: " << MemberFuncName << "\n";
  }

  jobject ProtocolNode = Wala->makeConstant(ProtocolName.c_str());

  jobject MemberNameNode = Wala->makeConstant(MemberFuncName.c_str());  
  jobject FuncNode = Wala->makeNode(CAstWrapper::FUNCTION_EXPR, MemberNameNode);

  jobject Node = Wala->makeNode(CAstWrapper::OBJECT_REF, ProtocolNode , FuncNode );

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(WMI), Node));

  return Node;
}

/*******************************************************************************/
/*                              FUNCTION APPLICATION                           */
/*******************************************************************************/

jobject SILWalaInstructionVisitor::visitApplyInst(ApplyInst *AI) {
  if (auto Node = visitApplySite(AI)) {
    NodeMap.insert(std::make_pair(static_cast<ValueBase *>(AI), Node)); // insert the node into the hash map
    return Node;
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}   

jobject SILWalaInstructionVisitor::visitBeginApplyInst(BeginApplyInst *BAI) {
  if (auto Node = visitApplySite(BAI)) {
    NodeMap.insert(std::make_pair(BAI, Node)); // insert the node into the hash map
    return Node;
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitEndApplyInst(EndApplyInst *EAI) {
  if (Print) {
    llvm::outs() << "[BEGIN APPLY]: " << EAI->getBeginApply() << "\n";
  }

  findAndRemoveCAstNode(EAI->getBeginApply());

  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitAbortApplyInst(AbortApplyInst *AAI) {
  if (Print) {
    llvm::outs() << "[BEGIN APPLY]: " << AAI->getBeginApply() << "\n";
  }

  findAndRemoveCAstNode(AAI->getBeginApply());

  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitPartialApplyInst(PartialApplyInst *PAI) {
  if (auto Node = visitApplySite(PAI)) {
    NodeMap.insert(std::make_pair(static_cast<ValueBase *>(PAI), Node)); // insert the node into the hash map
    return Node;
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitApplySite(ApplySite Apply) {
  jobject Node = Wala->makeNode(CAstWrapper::EMPTY); // the CAst node to be created
  auto *Callee = Apply.getReferencedFunction();

  if (!Callee) {
    return Wala->makeNode(CAstWrapper::EMPTY);
  }

  auto *FD = Callee->getLocation().getAsASTNode<FuncDecl>();

  if (Print) {
    llvm::outs() << "\t [CALLEE]: " << Demangle::demangleSymbolAsString(Callee->getName()) << "\n";
    for (unsigned I = 0; I < Apply.getNumArguments(); ++I) {
      SILValue V = Apply.getArgument(I);
      llvm::outs() << "\t [ARG] #" << I << ": " << V;
      llvm::outs() << "\t [ADDR] #" << I << ": " << V.getOpaqueValue() << "\n";
    }
  }

  if (FD && (FD->isUnaryOperator() || FD->isBinaryOperator())) {
    Identifier name = FD->getName();
    jobject OperatorNode = getOperatorCAstType(name);
    if (OperatorNode != nullptr) {
      llvm::outs() << "\t Built in operator\n";
      auto GetOperand = [&Apply, this](unsigned int Index) -> jobject {
        if (Index < Apply.getNumArguments()) {
          SILValue Argument = Apply.getArgument(Index);
          return findAndRemoveCAstNode(Argument.getOpaqueValue());
        }
        else return Wala->makeNode(CAstWrapper::EMPTY);
      };
      if (FD->isUnaryOperator()) {
        Node = Wala->makeNode(CAstWrapper::UNARY_EXPR, OperatorNode, GetOperand(0));
      } else {
        Node = Wala->makeNode(CAstWrapper::BINARY_EXPR, OperatorNode, GetOperand(0), GetOperand(1));
      }
      return Node;
    } // otherwise, fall through to the regular funcion call logic
  }

  auto FuncExprNode = findAndRemoveCAstNode(Callee);
  list<jobject> Params;

  for (unsigned i = 0; i < Apply.getNumArguments(); ++i) {
    SILValue Arg = Apply.getArgument(i);
    jobject Child = findAndRemoveCAstNode(Arg.getOpaqueValue());
    if (Child != nullptr) {
      Params.push_back(Child);
    }
  }

  Node = Wala->makeNode(CAstWrapper::CALL, FuncExprNode, Wala->makeArray(&Params));
  return Node;
}

jobject SILWalaInstructionVisitor::visitBuiltinInst(BuiltinInst *BI) {

  list<jobject> params;

  string FuncName = BI->getName().str();
  if (FuncName.empty()) {
    // cannot get function name, abort
    return Wala->makeNode(CAstWrapper::EMPTY);
  }

  // To prevent confusion if there is a user defined func with the same name
  FuncName = "Builtin." + FuncName;

  if (Print) {
    llvm::outs() << "Builtin Function Name: " << FuncName << "\n";
  }

  jobject NameNode = Wala->makeConstant(FuncName.c_str());
  jobject FuncExprNode = Wala->makeNode(CAstWrapper::FUNCTION_EXPR, NameNode);

  for (const auto &operand : BI->getArguments()) {
    if (Print) {
      llvm::outs() << "\t [OPERAND]: " << operand << "\n";
    }
    jobject child = findAndRemoveCAstNode(operand);
    if (child != nullptr) {
      params.push_back(child);
    }
  }

  jobject Node = Wala->makeNode(CAstWrapper::CALL, FuncExprNode, Wala->makeArray(&params));

  return Node;
}

/*******************************************************************************/
/*                                  METATYPES                                  */
/*******************************************************************************/

jobject SILWalaInstructionVisitor::visitMetatypeInst(MetatypeInst *MI) {

  string MetatypeName = MI->getType().getAsString();

  if (Print) {
    llvm::outs() << "\t [METATYPE]: " << MetatypeName << "\n";
  }

  jobject NameNode = Wala->makeConstant(MetatypeName.c_str());
  jobject MetaTypeConstNode = Wala->makeNode(CAstWrapper::CONSTANT, NameNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(MI), MetaTypeConstNode));

  return MetaTypeConstNode;
}

jobject SILWalaInstructionVisitor::visitValueMetatypeInst(ValueMetatypeInst *VMI) {

  auto ValueMetatypeOperand = VMI->getOperand();

  if (Print) {
    llvm::outs() << "\t [METATYPE]: " << VMI->getType().getAsString() << "\n";
  }

  jobject TypeNode = findAndRemoveCAstNode(ValueMetatypeOperand.getOpaqueValue());

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(VMI), TypeNode));
  
  return TypeNode;
}                     

/*******************************************************************************/
/*                                AGGREGATE TYPES                              */
/*******************************************************************************/

jobject SILWalaInstructionVisitor::visitCopyValueInst(CopyValueInst *CVI) {
  llvm::outs() << "\t [OPERAND]:" << CVI->getOperand() << "\n";
  jobject Node = findAndRemoveCAstNode(CVI->getOperand().getOpaqueValue());

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(CVI), Node));
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitDestroyValueInst(DestroyValueInst *DVI) {

  SILValue DestroyValue = DVI->getOperand();

  if (Print) {
      llvm::outs() << "\t [VALUE TO DESTROY]: " << DestroyValue.getOpaqueValue() << "\n";
  }

  findAndRemoveCAstNode(DestroyValue.getOpaqueValue());
  SymbolTable.remove(DestroyValue.getOpaqueValue());

  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitTupleInst(TupleInst *TI) {

  list<jobject> Properties;

  jobject TupleIdentifierNode = Wala->makeConstant("Tuple");

  Properties.push_back(TupleIdentifierNode);

  for (Operand &TupleOperand : TI->getElementOperands()) {

      SILValue Value = TupleOperand.get();
      unsigned ValueNumber = TupleOperand.getOperandNumber();

      jobject OperandNameNode = Wala->makeConstant(std::to_string(ValueNumber).c_str());
      jobject OperandValueNode = findAndRemoveCAstNode(Value.getOpaqueValue());

      if (Print) {
        llvm::outs() << "\t [OPERAND]: " << ValueNumber << " [VALUE]: "<< Value.getOpaqueValue()  << "\n";
      }

      Properties.push_back(OperandNameNode);
      Properties.push_back(OperandValueNode);
  }

  auto VisitTupleNode = Wala->makeNode(CAstWrapper::OBJECT_LITERAL, Wala->makeArray(&Properties));

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(TI), VisitTupleNode));

  return VisitTupleNode;
}  

jobject SILWalaInstructionVisitor::visitTupleExtractInst(TupleExtractInst *TEI) {
  // Operand is a tuple type from which some field needs to be extracted.
  // So it must have been created earlier. We just find the corresponding node
  SILValue TupleTypeOperand = TEI->getOperand();
  jobject TupleTypeNode = findAndRemoveCAstNode(TupleTypeOperand.getOpaqueValue());

  // Now we get the element
  const TupleTypeElt &Element = TEI->getTupleType()->getElement(TEI->getFieldNo());

  // We would need two information to create a node resembling this instruction - (object, field)
  // First information, object, is already available as TupleTypeNode
  // Need to find the second information, field.
  // Then would create new node with the field name or field type name
  // Depending on whether field name is available or not
  jobject FieldNode = Wala->makeNode(CAstWrapper::EMPTY);
  if(Element.hasName()){
    string FieldName = Element.getName().str();
    FieldNode = Wala->makeConstant(FieldName.c_str());
  }
  else{
    string FieldTypeName = Element.getType().getString();
    FieldNode = Wala->makeConstant(FieldTypeName.c_str());
  }

  // Now create a new Wala node of OBJECT_REF type with these two nodes
  jobject TupleExtractNode = Wala->makeNode(CAstWrapper::OBJECT_REF, TupleTypeNode, FieldNode);

  //And insert the new node to the NodeMap
  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(TEI), TupleExtractNode));
  
  if (Print) {
    llvm::outs() << "\t [OPERAND ADDR]: " << TupleTypeOperand.getOpaqueValue() << "\n";
    if(Element.hasName()){
      llvm::outs() << "\t [TUPLE FIELD NAME]: " << Element.getName().str() << "\n";
    }
    else{
      llvm::outs() << "\t [TUPLE FIELD TYPE NAME]: " << Element.getType().getString() << "\n";
    }
  }

  return TupleExtractNode;
}

jobject SILWalaInstructionVisitor::visitStructInst(StructInst *SI) {

  list<jobject> Fields;

  StringRef StructName = SI->getStructDecl()->getNameStr();

  jobject DiscriminantNameNode = Wala->makeConstant(StructName.data());

  llvm::outs() << "\t [STRUCT]: " << StructName <<  "\n";

  Fields.push_back(DiscriminantNameNode);

  for (Operand &StructOperand : SI->getElementOperands()) {

      unsigned OperandNumber = StructOperand.getOperandNumber();

      jobject OperandValueNode = findAndRemoveCAstNode(StructOperand.get().getOpaqueValue());
      jobject OperandNameNode = Wala->makeConstant(std::to_string(OperandNumber).c_str());

      if (Print) {
        llvm::outs() << "\t [OPERAND]: " << OperandNumber << " [VALUE]: "<< OperandValueNode  << "\n";
      }

      Fields.push_back(OperandNameNode);
      Fields.push_back(OperandValueNode);
  }

  auto VisitStructNode = Wala->makeNode(CAstWrapper::OBJECT_LITERAL, Wala->makeArray(&Fields));

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(SI), VisitStructNode));

  return VisitStructNode;
}
  
jobject SILWalaInstructionVisitor::visitStructExtractInst(StructExtractInst *SEI) {

  SILValue StructOperand = SEI->getOperand();

  StructDecl *StructElement = SEI->getStructDecl();
  VarDecl *StructField = SEI->getField();

  jobject ElementNode = findAndRemoveCAstNode(StructOperand.getOpaqueValue());

  if (Print) {
        llvm::outs() << "\t [OPERAND ADDR]: " << StructOperand.getOpaqueValue() << "\n";
        llvm::outs() << "\t [STRUCT]: " << StructElement->getDeclaredType().getString() << "\n";
        llvm::outs() << "\t [STRUCT FIELD]: " << StructField->getNameStr() << "\n";
  }

  string FieldName = StructField->getNameStr();
  jobject FieldNameNode = Wala->makeConstant(FieldName.c_str());
  jobject FieldNode = Wala->makeNode(CAstWrapper::VAR, FieldNameNode);
  jobject Node = Wala->makeNode(CAstWrapper::OBJECT_REF, ElementNode , FieldNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(SEI), Node));

  return Node;
}

jobject SILWalaInstructionVisitor::visitTupleElementAddrInst(TupleElementAddrInst *TEAI) {
  SILValue TupleTypeOperand = TEAI->getOperand();
  jobject TupleTypeNode = findAndRemoveCAstNode(TupleTypeOperand.getOpaqueValue());

  const TupleTypeElt &Element = TEAI->getTupleType()->getElement(TEAI->getFieldNo());

  jobject FieldNode = Wala->makeNode(CAstWrapper::EMPTY);
  if(Element.hasName()){
    string FieldName = Element.getName().str();
    FieldNode = Wala->makeConstant(FieldName.c_str());
  }
  else{
    string FieldTypeName = Element.getType().getString();
    FieldNode = Wala->makeConstant(FieldTypeName.c_str());
  }

  jobject TupleElementAddrNode = Wala->makeNode(CAstWrapper::OBJECT_REF, TupleTypeNode, FieldNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(TEAI), TupleElementAddrNode));
  
  if (Print) {
    llvm::outs() << "\t [OPERAND ADDR]: " << TupleTypeOperand.getOpaqueValue() << "\n";
    if(Element.hasName()){
      llvm::outs() << "\t [TUPLE FIELD NAME]: " << Element.getName().str() << "\n";
    }
    else{
      llvm::outs() << "\t [TUPLE FIELD TYPE NAME]: " << Element.getType().getString() << "\n";
    }
  }

  return TupleElementAddrNode;
}

jobject SILWalaInstructionVisitor::visitStructElementAddrInst(StructElementAddrInst *SEAI) {

  SILValue StructOperand = SEAI->getOperand();

  StructDecl *StructElement = SEAI->getStructDecl();
  VarDecl *StructField = SEAI->getField();

  jobject ElementNode = findAndRemoveCAstNode(StructOperand.getOpaqueValue());

  if (Print) {
        llvm::outs() << "\t [OPERAND ADDR]: " << StructOperand.getOpaqueValue() << "\n";
        llvm::outs() << "\t [STRUCT]: " << StructElement->getDeclaredType().getString() << "\n";
        llvm::outs() << "\t [STRUCT FIELD]: " << StructField->getNameStr() << "\n";
  }

  string FieldName = StructField->getNameStr();
  jobject FieldNameNode = Wala->makeConstant(FieldName.c_str());
  jobject FieldNode = Wala->makeNode(CAstWrapper::VAR, FieldNameNode);

  jobject Node = Wala->makeNode(CAstWrapper::OBJECT_REF, ElementNode , FieldNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(SEAI), Node));

  return Node;
}

jobject SILWalaInstructionVisitor::visitRefElementAddrInst(RefElementAddrInst *REAI) {

  SILValue ElementOperand = REAI->getOperand();

  ClassDecl *ClassElement = REAI->getClassDecl();
  VarDecl *ClassField = REAI->getField();
  
  jobject ElementNode = findAndRemoveCAstNode(ElementOperand.getOpaqueValue());

  if (Print) {
        llvm::outs() << "\t [OPERAND]: " << ElementOperand << "\n";
        llvm::outs() << "\t [CLASS]: " << ClassElement->getDeclaredType().getString() << "\n";
        llvm::outs() << "\t [CLASS FIELD]: " << ClassField->getNameStr() << "\n";
  }

  string ClassName = ClassField->getNameStr();
  jobject FieldNameNode = Wala->makeConstant(ClassName.c_str());
  jobject FieldNode = Wala->makeNode(CAstWrapper::VAR, FieldNameNode);

  // OBJECT_REF takes (CLASS , FIELD)
  auto Node = Wala->makeNode(CAstWrapper::OBJECT_REF, ElementNode , FieldNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(REAI), Node));

  return Node;
}

jobject SILWalaInstructionVisitor::visitRefTailAddrInst(RefTailAddrInst *RTAI) {
  SILValue ElementOperand = RTAI->getOperand();

  ClassDecl *ClassElement = RTAI->getClassDecl();
  SILType  ResultType = RTAI->getTailType();
  
  jobject ElementNode = findAndRemoveCAstNode(ElementOperand.getOpaqueValue());

  if (Print) {
    llvm::outs() << "\t [RefTailAddrInst]: " << static_cast<ValueBase *>(RTAI) << "\n";
    llvm::outs() << "\t [OPERAND]: " << ElementOperand << "\n";
    llvm::outs() << "\t [OPERAND ADDR]: " << ElementOperand.getOpaqueValue() << "\n";
    llvm::outs() << "\t [CLASS]: " << ClassElement->getDeclaredType().getString() << "\n";
    llvm::outs() << "\t [RESULT TYPE]: " << ResultType.getAsString() << "\n";
  }

  jobject ResultTypeNameNode = Wala->makeConstant(ResultType.getAsString().c_str());

  auto Node = Wala->makeNode(CAstWrapper::OBJECT_REF, ElementNode , ResultTypeNameNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(RTAI), Node));

  return Node;
}

/*******************************************************************************/
/*                                    ENUMS                                    */
/*******************************************************************************/

jobject SILWalaInstructionVisitor::visitEnumInst(EnumInst *EI) {
 
  list<jobject> Properties;

  StringRef enumName = EI->getElement()->getParentEnum()->getName().str();
  StringRef discriminantName = EI->getElement()->getNameStr();

  jobject DiscriminantNameNode = Wala->makeConstant(enumName.data());
  jobject DiscriminantValueNode = Wala->makeConstant(discriminantName.data());

  Properties.push_back(DiscriminantNameNode);
  Properties.push_back(DiscriminantValueNode);

  if (Print) {
    llvm::outs() << "\t [ENUM]: " << enumName <<  "\n";
    llvm::outs() << "\t [CASE]: " << discriminantName <<  "\n";
  }

  for (Operand &EnumOperand : EI->getAllOperands()) {

      unsigned OperandNumber = EnumOperand.getOperandNumber();

      jobject OperandValueNode = findAndRemoveCAstNode(EnumOperand.get().getOpaqueValue());
      jobject OperandNameNode = Wala->makeConstant(std::to_string(OperandNumber).c_str());

      if (Print) {
        llvm::outs() << "\t [OPERAND]: " << OperandNumber << " [VALUE]: "<< OperandValueNode  << "\n";
      }

      Properties.push_back(OperandNameNode);
      Properties.push_back(OperandValueNode);
  }

  auto VisitEnumNode = Wala->makeNode(CAstWrapper::OBJECT_LITERAL, Wala->makeArray(&Properties));

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(EI), VisitEnumNode));

  return VisitEnumNode;
 }

jobject SILWalaInstructionVisitor::visitUncheckedEnumDataInst(UncheckedEnumDataInst *UED) {

  SILValue Value = UED->getOperand();

  if (Print) {
    llvm::outs() << "\t [ENUM]: " << UED->getEnumDecl()->getName() << "\n";
    llvm::outs() << "\t [CASE]: " << UED->getElement()->getNameStr() << "\n";
    llvm::outs() << "\t [OPERAND]: " << Value.getOpaqueValue() << "\n";
  }
  
  jobject UncheckedEnumData = findAndRemoveCAstNode(Value.getOpaqueValue());
  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(UED), UncheckedEnumData));

  return UncheckedEnumData;
}

jobject SILWalaInstructionVisitor::visitInitEnumDataAddrInst(InitEnumDataAddrInst *UDAI) {

  list<jobject> Properties;

  SILValue EnumOperand = UDAI->getOperand();
  StringRef EnumName = UDAI->getElement()->getParentEnum()->getName().str();
  
  jobject EnumNameNode = Wala->makeConstant(EnumName.data());

  Properties.push_back(EnumNameNode);

  StringRef CaseName = UDAI->getElement()->getNameStr().str();
  jobject CaseNameNode = Wala->makeConstant(CaseName.data());

  jobject CaseNode = findAndRemoveCAstNode(EnumOperand.getOpaqueValue());

  Properties.push_back(CaseNameNode);
  Properties.push_back(CaseNode);

  if (Print) {
    llvm::outs() << "\t [ENUM]: " << EnumName <<  "\n";
    llvm::outs() << "\t [CASE]: " << CaseName <<  "\n";
    llvm::outs() << "\t [CASE NODE]: " << CaseNode <<  "\n";
  }

  jobject InitEnumNode = Wala->makeNode(CAstWrapper::OBJECT_LITERAL, Wala->makeArray(&Properties));

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(UDAI), InitEnumNode));

  return InitEnumNode;
}

jobject SILWalaInstructionVisitor::visitUncheckedTakeEnumDataAddrInst(UncheckedTakeEnumDataAddrInst *UDAI) {

  SILValue EnumletOperand = UDAI->getOperand();
  EnumElementDecl *EnumElement = UDAI->getElement();
  EnumDecl *EnumDecl = UDAI->getEnumDecl();

  string EnumletName = EnumElement->getName().str();
  string EnumName = EnumDecl->getNameStr().str();

  string FullEnumCaseName = EnumName + "." + EnumletName;

  if (Print) {
    llvm::outs() << "\t [OPERAND]: " << EnumletOperand << "\n";
    llvm::outs() << "\t [OPERAND ADDR]: " << EnumletOperand.getOpaqueValue() << "\n";
    llvm::outs() << "\t [ENUM]: " << EnumName << "\n";
    llvm::outs() << "\t [ENUMLET]: " << EnumletName << "\n";
  }

  jobject EnumNode = findAndRemoveCAstNode(EnumletOperand.getOpaqueValue());
  jobject CaseNode =  Wala->makeConstant(FullEnumCaseName.c_str());
  jobject ReferenceNode = Wala->makeNode(CAstWrapper::OBJECT_REF, EnumNode , CaseNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(UDAI), ReferenceNode));

   return ReferenceNode;
}

jobject SILWalaInstructionVisitor::visitSelectEnumInst(SelectEnumInst *SEI) {

  list<jobject> Children;

  SILValue Cond = SEI->getEnumOperand();

  jobject CondNode = findAndRemoveCAstNode(Cond.getOpaqueValue());
  jobject DescriminatorNameNode = Wala->makeConstant("DISCRIMINATOR");

  if (Print) {
    llvm::outs() << "\t [COND]: " << Cond << "\n";
    llvm::outs() << "\t [COND NODE]: " << Cond.getOpaqueValue() << "\n";
  }

  Children.push_back(DescriminatorNameNode);
  Children.push_back(CondNode);

  for (unsigned Idx = 0, Num = SEI->getNumCases(); Idx < Num; ++Idx) {
    auto Case = SEI->getCase(Idx);

    EnumElementDecl *CaseDecl = Case.first;
    // SILValue CaseVal = Case.second;

    StringRef EnumName = CaseDecl->getParentEnum()->getName().str();

    for (EnumElementDecl *E : CaseDecl->getParentEnum()->getAllElements()) {
      
      StringRef CaseName = E->getNameStr();

      SILValue CaseVal = SEI->getCaseResult(E);
      if (auto intLit = dyn_cast<IntegerLiteralInst>(CaseVal)) {

        auto CaseNameString = EnumName.str() + "." + CaseName.str() + ".enumlet!." + intLit->getValue().toString(10, false);

        jobject CaseNameNode = Wala->makeConstant(CaseNameString.c_str());
        jobject CaseValNode = findAndRemoveCAstNode(CaseVal);

        if (Print) {
          llvm::outs() << "\t [CASE NAME]: " << CaseNameString << "\n";
          llvm::outs() << "\t [CASE VAL]: " << CaseValNode << "\n";
        }

        Children.push_back(CaseNameNode);
        Children.push_back(CaseValNode);

      }
    }
  }

  auto SelectEnumNode = Wala->makeNode(CAstWrapper::BLOCK_STMT,  Wala->makeArray(&Children));
  auto SelectNode = Wala->makeNode(CAstWrapper::SWITCH, CondNode, SelectEnumNode);

  NodeMap.insert(std::make_pair(SEI, SelectNode));

  return SelectNode;
}

/*******************************************************************************/
/*                      PROTOCOL AND PROTOCOL COMPARISON TYPES                 */
/*******************************************************************************/

jobject SILWalaInstructionVisitor::visitInitExistentialAddrInst(InitExistentialAddrInst *IEAI) {
  if (Print) {
    llvm::outs() << "[IEAI] " << IEAI << "\n";
    llvm::outs() << "[OPERAND]: " << IEAI->getOperand() << "\n";
    llvm::outs() << "[FormalConcreteType]: " << IEAI->getFormalConcreteType() << "\n";
  }

  if (SymbolTable.has(IEAI->getOperand().getOpaqueValue())) {
    auto name = "ExistentialAddr of " + 
      SymbolTable.get(IEAI->getOperand().getOpaqueValue()) + " -> " + 
      IEAI->getFormalConcreteType().getString();
    SymbolTable.insert(static_cast<ValueBase *>(IEAI), name);
    }

  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitInitExistentialValueInst(InitExistentialValueInst *IEVI) {
  if (Print) {
    llvm::outs() << "[IEVI]: " << IEVI << "\n";
    llvm::outs() << "[OPERAND]: " << IEVI->getOperand() << "\n";
    llvm::outs() << "[ConcreteType]: " << IEVI->getFormalConcreteType() << "\n";
  }

   if (SymbolTable.has(IEVI->getOperand().getOpaqueValue())) {
    auto name = "ExistentialValue of " + 
      SymbolTable.get(IEVI->getOperand().getOpaqueValue()) + " -> " + 
      IEVI->getFormalConcreteType().getString();
    SymbolTable.insert(static_cast<ValueBase *>(IEVI), name);
  }

  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitDeinitExistentialAddrInst(DeinitExistentialAddrInst *DEAI) {
  if (Print) {
    llvm::outs() << "[OPERAND]: " << DEAI->getOperand() << "\n";
  }

  findAndRemoveCAstNode(DEAI->getOperand().getOpaqueValue());
  SymbolTable.remove(DEAI->getOperand().getOpaqueValue());

  return Wala->makeNode(CAstWrapper::EMPTY);
}


jobject SILWalaInstructionVisitor::visitDeinitExistentialValueInst(DeinitExistentialValueInst *DEVI) {
  if (Print) {
    llvm::outs() << "[DEVI]: " << DEVI << "\n";
    llvm::outs() << "[Operand]: " << DEVI->getOperand() << "\n";
  }

  findAndRemoveCAstNode(DEVI->getOperand().getOpaqueValue());
  SymbolTable.remove(DEVI->getOperand().getOpaqueValue());

  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitOpenExistentialAddrInst(OpenExistentialAddrInst *OEAI) {
  jobject operandNode = findAndRemoveCAstNode(OEAI->getOperand().getOpaqueValue());
  string openedType = OEAI->getType().getAsString();

  if (Print) {
    llvm::outs() << "[OPERAND]: " << OEAI->getOperand() << "\n";
    llvm::outs() << "[EXISTENTIAL TYPE]: " << openedType << "\n";
  }

  jobject openedTypeNode = Wala->makeConstant(openedType.c_str());
  jobject castNode = Wala->makeNode(CAstWrapper::CAST, operandNode, openedTypeNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(OEAI), castNode));

  return castNode;
}

jobject SILWalaInstructionVisitor::visitOpenExistentialValueInst(OpenExistentialValueInst *OEVI) {
  jobject operandNode = findAndRemoveCAstNode(OEVI->getOperand().getOpaqueValue());
  string openedType = OEVI->getType().getAsString();

  if (Print) {
    llvm::outs() << "[OPERAND]: " << OEVI->getOperand() << "\n";
    llvm::outs() << "[EXISTENTIAL TYPE]: " << openedType << "\n";
  }

  jobject openedTypeNode = Wala->makeConstant(openedType.c_str());
  jobject castNode = Wala->makeNode(CAstWrapper::CAST, operandNode, openedTypeNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(OEVI), castNode));

  return castNode;
}


jobject SILWalaInstructionVisitor::visitInitExistentialMetatypeInst(InitExistentialMetatypeInst *IEMI) {
  if (Print) {
    llvm::outs() << "[IEMI]: " << IEMI << "\n";
    llvm::outs() << "[OPERAND]: " << IEMI->getOperand() << "\n";
    llvm::outs() << "[EX-TYPE]: " << IEMI->getType() << "\n";
  }

  if (SymbolTable.has(IEMI->getOperand().getOpaqueValue())) {
    auto name = "ExistentialMetatype of " + 
      SymbolTable.get(IEMI->getOperand().getOpaqueValue()) + " -> " + 
      IEMI->getType().getAsString();
    SymbolTable.insert(static_cast<ValueBase *>(IEMI), name);
  }

  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitOpenExistentialMetatypeInst(OpenExistentialMetatypeInst *OEMI) {
  jobject operandNode = findAndRemoveCAstNode(OEMI->getOperand().getOpaqueValue());
  string openedType = OEMI->getType().getAsString();

  if (Print) {
    llvm::outs() << "[OPERAND]: " << OEMI->getOperand() << "\n";
    llvm::outs() << "[EXISTENTIAL TYPE]: " << openedType << "\n";
  }

  jobject openedTypeNode = Wala->makeConstant(openedType.c_str());
  jobject castNode = Wala->makeNode(CAstWrapper::CAST, operandNode, openedTypeNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(OEMI), castNode));

  return castNode;
}

jobject SILWalaInstructionVisitor::visitAllocExistentialBoxInst(AllocExistentialBoxInst *AEBI) {    
    if (Print) {
      llvm::outs() << "\t [AEBI]: " << AEBI << "\n";
      llvm::outs() << "\t [CONTRETE TYPE]: " << AEBI->getFormalConcreteType() << "\n";
      llvm::outs() << "\t [EXISTENTIAL TYPE]: " << AEBI->getExistentialType() << "\n";
    }

    auto name = "ExistentialBox:" + 
      AEBI->getFormalConcreteType().getString() + "->" + AEBI->getExistentialType().getAsString();
    SymbolTable.insert(static_cast<ValueBase *>(AEBI), name);

    return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitProjectExistentialBoxInst(ProjectExistentialBoxInst *PEBI) {
  if (Print) {
    llvm::outs() << "\t [PEBI]: " << PEBI << "\n";
    llvm::outs() << "\t [OPERAND]: " << PEBI->getOperand() << "\n";
    llvm::outs() << "\t [OPERAND ADDR]: " << PEBI->getOperand().getOpaqueValue() << "\n";
  }
  // NOTE: Apple documentation states: This instruction has undefined behavior if the box is not currently allocated
  //       (link: https://github.com/apple/swift/blob/master/docs/SIL.rst#project-existential-box so there is no need to allocate
  //       it if it is not currently in the Symbol Table
  if (SymbolTable.has(PEBI->getOperand().getOpaqueValue())) {
    SymbolTable.duplicate(static_cast<ValueBase *>(PEBI), SymbolTable.get(PEBI->getOperand().getOpaqueValue()).c_str());
  }

  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitOpenExistentialBoxInst(OpenExistentialBoxInst *OEBI) {
  jobject operandNode = findAndRemoveCAstNode(OEBI->getOperand().getOpaqueValue());
  string openedType = OEBI->getType().getAsString();

  if (Print) {
    llvm::outs() << "[OPERAND]: " << OEBI->getOperand() << "\n";
    llvm::outs() << "[EXISTENTIAL TYPE]: " << openedType << "\n";
  }

  jobject openedTypeNode = Wala->makeConstant(openedType.c_str());
  jobject castNode = Wala->makeNode(CAstWrapper::CAST, operandNode, openedTypeNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(OEBI), castNode));

  return castNode;
}

jobject SILWalaInstructionVisitor::visitOpenExistentialBoxValueInst(OpenExistentialBoxValueInst *OEBVI) {
  jobject operandNode = findAndRemoveCAstNode(OEBVI->getOperand().getOpaqueValue());
  string openedType = OEBVI->getType().getAsString();

  if (Print) {
    llvm::outs() << "[OPERAND]: " << OEBVI->getOperand() << "\n";
    llvm::outs() << "[EXISTENTIAL TYPE]: " << openedType << "\n";
  }

  jobject openedTypeNode = Wala->makeConstant(openedType.c_str());
  jobject castNode = Wala->makeNode(CAstWrapper::CAST, operandNode, openedTypeNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(OEBVI), castNode));

  return castNode;
}

jobject SILWalaInstructionVisitor::visitDeallocExistentialBoxInst(DeallocExistentialBoxInst *DEBI) {
  if (Print) {
    llvm::outs() << "\t [OPERAND]: " << DEBI->getOperand() << "\n";
  }

  findAndRemoveCAstNode(DEBI->getOperand().getOpaqueValue());
  SymbolTable.remove(DEBI->getOperand().getOpaqueValue());

  return Wala->makeNode(CAstWrapper::EMPTY);
}


/*******************************************************************************/
/*                        BLOCKS                                               */
/*******************************************************************************/

/*******************************************************************************/
/*                  UNCHECKED CONVERSIONS                                      */
/*******************************************************************************/

jobject SILWalaInstructionVisitor::visitUpcastInst(UpcastInst *UI) {
  SILValue ConvertedValue = UI->getConverted();
  string CovertedType = UI->getType().getAsString();

  jobject UpcastNode = findAndRemoveCAstNode(ConvertedValue.getOpaqueValue());

  if (Print) {
    llvm::outs() << "\t [CONVERTED ADDR]: " << ConvertedValue.getOpaqueValue() << " [TO]: " << CovertedType << "\n";
    llvm::outs() << "\t [CONVERTED NODE]: " << UpcastNode << "\n";
  }

  jobject ConvertedTypeNode = Wala->makeConstant(CovertedType.c_str());
  jobject CastedNode = Wala->makeNode(CAstWrapper::CAST, UpcastNode, ConvertedTypeNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(UI), CastedNode));

  return CastedNode;
}

jobject SILWalaInstructionVisitor::visitAddressToPointerInst(AddressToPointerInst *ATPI) {

  SILValue ConvertedValue = ATPI->getConverted();
  string CovertedType = ATPI->getType().getAsString();

  jobject ConvertedNode = findAndRemoveCAstNode(ConvertedValue.getOpaqueValue());

  if (Print) {
    llvm::outs() << "\t [CONVERTED ADDR]: " << ConvertedValue.getOpaqueValue() << " [TO]: " << CovertedType << "\n";
    llvm::outs() << "\t [CONVERTED NODE]: " << ConvertedNode << "\n";
  }

  jobject ConvertedTypeNode = Wala->makeConstant(CovertedType.c_str());
  jobject RawPointerCastNode = Wala->makeNode(CAstWrapper::CAST, ConvertedNode, ConvertedTypeNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(ATPI), RawPointerCastNode));
  return RawPointerCastNode;
}

jobject SILWalaInstructionVisitor::visitUncheckedRefCastInst(UncheckedRefCastInst *URCI) {

  SILValue ConvertedValue = URCI->getConverted();
  string CovertedType = URCI->getType().getAsString();

  jobject UncheckedRefCastNode = findAndRemoveCAstNode(ConvertedValue.getOpaqueValue());

  if (Print) {
    llvm::outs() << "\t [CONVERTED ADDR]: " << ConvertedValue.getOpaqueValue() << " [TO]: " << CovertedType << "\n";
    llvm::outs() << "\t [CONVERTED NODE]: " << UncheckedRefCastNode << "\n";
  }

  jobject ConvertedTypeNode = Wala->makeConstant(CovertedType.c_str());
  jobject CastedNode = Wala->makeNode(CAstWrapper::CAST, UncheckedRefCastNode, ConvertedTypeNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(URCI), CastedNode));

  return CastedNode;
}

jobject SILWalaInstructionVisitor::visitRawPointerToRefInst(RawPointerToRefInst *CI) {

  SILValue ValueToBeConverted = CI->getConverted();
  string TypeToBeConvertedInto = CI->getType().getAsString();

  if (Print) {
    llvm::outs() << "\t [RawPointerToRef]: " << static_cast<ValueBase *>(CI) << "\n";
    llvm::outs() << "\t " << ValueToBeConverted.getOpaqueValue() << " [TO BE CONVERTED INTO]: " << TypeToBeConvertedInto << "\n";
  }
  
  jobject ToBeConvertedNode = findAndRemoveCAstNode(ValueToBeConverted.getOpaqueValue());

  jobject TypeNode = Wala->makeConstant(TypeToBeConvertedInto.c_str());

  jobject ConversionNode = Wala->makeNode(CAstWrapper::CAST, ToBeConvertedNode, TypeNode);

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(CI), ConversionNode));

  return ConversionNode;
}

jobject SILWalaInstructionVisitor::visitPointerToAddressInst(PointerToAddressInst *PTAI) {
  // Getting the value to be converted
  SILValue ValueToBeConverted = PTAI->getConverted();
  // Now getting the type of address to be converted into
  string TypeToBeConvertedInto = PTAI->getType().getAsString();
  
  // Conversion means it is a "CAST" instruction. CAST type node in Wala takes two other nodes.
  // One is a node corresponding to the value to be converted
  // Another is a node corresponding to the type to be converted into
  
  // Firstly finding the node corresponding to the value to be converted
  jobject ToBeConvertedNode = findAndRemoveCAstNode(ValueToBeConverted.getOpaqueValue());
  // Then creating the node corresponding to the type to be converted into
  jobject TypeNode = Wala->makeConstant(TypeToBeConvertedInto.c_str());
  // Then creating a new node of CAST type with these two nodes
  jobject ConversionNode = Wala->makeNode(CAstWrapper::CAST, ToBeConvertedNode, TypeNode);
  // Finally inserting into the NodeMap for future reference
  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(PTAI), ConversionNode));

  if (Print) {
    llvm::outs() << "\t" << ValueToBeConverted.getOpaqueValue() << " [TO BE CONVERTED INTO]: " << TypeToBeConvertedInto << " [TYPE ADDRESS] " << "\n";
  }

  return ConversionNode;
}

jobject SILWalaInstructionVisitor::visitThinToThickFunctionInst(ThinToThickFunctionInst *TTFI) {
  // Cast the instr to access methods

  SILValue CalleeValue = TTFI->getCallee();
  string CovertedType = TTFI->getType().getAsString();

  jobject FuncRefNode = findAndRemoveCAstNode(CalleeValue.getOpaqueValue());

  if (Print) {
    llvm::outs() << "\t [CALLEE ADDR]: " << CalleeValue.getOpaqueValue()  << " [TO]: " << CovertedType << "\n";
    llvm::outs() << "\t [CALLEE NODE]: " << FuncRefNode << "\n";
  }
  
  // cast in CASt
  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(TTFI), FuncRefNode));
  return FuncRefNode;
}

jobject SILWalaInstructionVisitor::visitThinFunctionToPointerInst(ThinFunctionToPointerInst *TFPI) {

   SILValue ConvertedFunction = TFPI->getConverted();
   string CovertedType = TFPI->getType().getAsString();
 
   jobject FunctionPointerNode = findAndRemoveCAstNode(ConvertedFunction.getOpaqueValue());
 
   if (Print) {
     llvm::outs() << "\t [FUNCTION ADDR]: " << ConvertedFunction.getOpaqueValue() << " [TO]: " << CovertedType << "\n";
     llvm::outs() << "\t [FUNCTION NODE]: " << FunctionPointerNode << "\n";
   }
 
   NodeMap.insert(std::make_pair(static_cast<ValueBase *>(TFPI), FunctionPointerNode));
 
   return FunctionPointerNode;
}

jobject SILWalaInstructionVisitor::visitConvertFunctionInst(ConvertFunctionInst *CFI) {

  SILValue ConvertedValue = CFI->getConverted();
  string CovertedType = CFI->getType().getAsString();

  jobject ConvertedFunctionNode = findAndRemoveCAstNode(ConvertedValue.getOpaqueValue());

  if (Print) {
    llvm::outs() << "\t [CONVERTED ADDR]: " << ConvertedValue.getOpaqueValue()  << " [TO]: " << CovertedType << "\n";
    llvm::outs() << "\t [CONVERTED NODE]: " << ConvertedFunctionNode << "\n";
  }

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(CFI), ConvertedFunctionNode));
  return ConvertedFunctionNode;
}

jobject SILWalaInstructionVisitor::visitUncheckedOwnershipConversionInst(UncheckedOwnershipConversionInst *UOCI) {

  SILValue ConversionOperand = UOCI->getOperand();
  string ConversionType = UOCI->getType().getAsString();

  jobject ConversionNode = findAndRemoveCAstNode(ConversionOperand.getOpaqueValue());

  if (Print) {
    llvm::outs() << "\t [CONVERTED ADDR]: " << ConversionOperand.getOpaqueValue() << "\n";
    llvm::outs() << "\t [CONVERTED NODE]: " << ConversionNode << "\n";
  }

  NodeMap.insert(std::make_pair(static_cast<ValueBase *>(UOCI), ConversionNode));
  return ConversionNode;
}

/*******************************************************************************/
/*                   CHECKED CONVERSIONS                                       */
/*******************************************************************************/

/*******************************************************************************/
/*                   RUNTIME FAILURES                                          */
/*******************************************************************************/

jobject SILWalaInstructionVisitor::visitCondFailInst(CondFailInst *FI) {

  SILValue CondOperand = FI->getOperand();

  if (Print) {
    llvm::outs() << "\t [OPERAND]: " << CondOperand << "\n";
    llvm::outs() << "\t [OPERAND ADDR]: " << CondOperand.getOpaqueValue() << "\n";
  }

  jobject Node = Wala->makeNode(CAstWrapper::ASSERT, findAndRemoveCAstNode(CondOperand.getOpaqueValue()));

  NodeMap.insert(std::make_pair(FI, Node));

  return Node;
}

/*******************************************************************************/
/*                      TERMINATORS                                            */
/*******************************************************************************/

jobject SILWalaInstructionVisitor::visitUnreachableInst(UnreachableInst *UI) {
  if (Print) {
    if (UI->isBranch()) {
      llvm::outs() << "\t This is a terminator of branch!" << "\n";
    }
    if (UI->isFunctionExiting()) {
      llvm::outs() << "\t This is a terminator of function!" << "\n";
    }
  }
  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitReturnInst(ReturnInst *RI) {
  SILValue RV = RI->getOperand();

  if (Print) {
    llvm::outs() << "\t [OPERAND]: " << RV << "\n";
    llvm::outs() << "\t [ADDR]: " << RV.getOpaqueValue() << "\n";
  }
  jobject Node = nullptr;
  if (RV != nullptr) {
    jobject V = nullptr;
    V = findAndRemoveCAstNode(RV.getOpaqueValue());
    if (V == nullptr) {
      Node = Wala->makeNode(CAstWrapper::RETURN);
    } else {
      Node = Wala->makeNode(CAstWrapper::RETURN, V);
    }
    NodeMap.insert(std::make_pair(RI, Node));
  }
  return Node;
}

jobject SILWalaInstructionVisitor::visitThrowInst(ThrowInst *TI) {
  SILValue TV = TI->getOperand();

  if (Print) {
    llvm::outs() << "\t [OPERAND]: " << TV << "\n";
    llvm::outs() << "\t [ADDR]: " << TV.getOpaqueValue() << "\n";
  }

  jobject Node = nullptr;
  if (TV != nullptr) {
    jobject V = nullptr;
    V = findAndRemoveCAstNode(TV.getOpaqueValue());
    if (V == nullptr) {
      Node = Wala->makeNode(CAstWrapper::THROW);
    } else {
      Node = Wala->makeNode(CAstWrapper::THROW, V);
    }
    NodeMap.insert(std::make_pair(TI, Node));
  }
  return Node;
}

jobject SILWalaInstructionVisitor::visitYieldInst(YieldInst *YI) {
  SILBasicBlock *ResumeBB = YI->getResumeBB();
  SILBasicBlock *UnwindBB = YI->getUnwindBB();

  if (Print) {
    llvm::outs() << "\t [RESUME BB]: " << ResumeBB << "\n";
    llvm::outs() << "\t [UNWIND BB]: " << UnwindBB << "\n";
  }

  list<jobject> yieldValues;

  for (const auto &value : YI->getYieldedValues()) {
    if (Print) {
      llvm::outs() << "\t [YIELD VALUE]: " << value << "\n";
    }
    jobject child = findAndRemoveCAstNode(value);
    if (child != nullptr) {
      yieldValues.push_back(child);
    }
  }

  jobject ResumeLabelNode = Wala->makeConstant(BasicBlockLabeller::label(ResumeBB).c_str());
  jobject ResumeGotoNode = Wala->makeNode(CAstWrapper::GOTO, ResumeLabelNode);

  jobject UnwindLabelNode = Wala->makeConstant(BasicBlockLabeller::label(UnwindBB).c_str());
  jobject UnwindGotoNode = Wala->makeNode(CAstWrapper::GOTO, UnwindLabelNode); 

  jobject Node = Wala->makeNode(CAstWrapper::YIELD_STMT, Wala->makeArray(&yieldValues), ResumeGotoNode, UnwindGotoNode);

  NodeMap.insert(std::make_pair(YI, Node));

  return Node;
}

jobject SILWalaInstructionVisitor::visitUnwindInst(UnwindInst *UI) {
  jobject Node = Wala->makeNode(CAstWrapper::UNWIND);
  NodeMap.insert(std::make_pair(UI, Node));
  return Node;
}

jobject SILWalaInstructionVisitor::visitBranchInst(BranchInst *BI) {
  // This is an unconditional branch
  jobject GotoNode = Wala->makeNode(CAstWrapper::EMPTY);

  // Destination block
  int I = 0;
  SILBasicBlock *Dest = BI->getDestBB();
  if (Print) {
    llvm::outs() << "\t [DESTBB]: " << Dest << "\n";
    if (Dest != nullptr) {
      for (auto &Instr : *Dest) {
        llvm::outs() << "\t [INST" << I++ << "]: " << &Instr << "\n";
      }
    }
  }
  if (Dest != nullptr) {
    jobject LabelNode = Wala->makeConstant(BasicBlockLabeller::label(Dest).c_str());
    GotoNode = Wala->makeNode(CAstWrapper::GOTO, LabelNode);
  }

  for (unsigned Idx = 0; Idx < BI->getNumArgs(); Idx++) {
    llvm::outs() << "[ADDR]: " << Dest->getArgument(Idx) << "\n";
    jobject Node = findAndRemoveCAstNode(BI->getArg(Idx).getOpaqueValue());
    SymbolTable.insert(Dest->getArgument(Idx), ("argument" + std::to_string(Idx)));

    jobject varName = Wala->makeConstant(SymbolTable.get(Dest->getArgument(Idx)).c_str());
    jobject var = Wala->makeNode(CAstWrapper::VAR, varName);
    jobject assign = Wala->makeNode(CAstWrapper::ASSIGN, var, Node);
    NodeList.push_back(assign);
  }
  return GotoNode;
}

jobject SILWalaInstructionVisitor::visitCondBranchInst(CondBranchInst *CBI) {
  // 1. Condition
  SILValue Cond = CBI->getCondition();
  jobject CondNode = findAndRemoveCAstNode(Cond.getOpaqueValue());
  if (Print) {
    llvm::outs() << "\t [COND]: " << Cond.getOpaqueValue() << "\n";
  }

  // 2. True block
  int I = 0;
  SILBasicBlock *TrueBasicBlock = CBI->getTrueBB();
  jobject TrueGotoNode = Wala->makeNode(CAstWrapper::EMPTY);
  if (Print) {
    llvm::outs() << "\t [TBB]: " << TrueBasicBlock << "\n";
    if (TrueBasicBlock != nullptr) {
      for (auto &Instr : *TrueBasicBlock) {
        llvm::outs() << "\t [INST" << I++ << "]: " << &Instr << "\n";
      }
    }
  }
  if (TrueBasicBlock != nullptr) {
    jobject LabelNode = Wala->makeConstant(BasicBlockLabeller::label(TrueBasicBlock).c_str());
    TrueGotoNode = Wala->makeNode(CAstWrapper::GOTO, LabelNode);
  }

  // 3. False block
  I = 0;
  SILBasicBlock *FalseBasicBlock = CBI->getFalseBB();
  jobject FalseGotoNode = Wala->makeNode(CAstWrapper::EMPTY);
  if (Print) {
    llvm::outs() << "\t [FBB]: " << FalseBasicBlock << "\n";
    if (FalseBasicBlock != nullptr) {
      for (auto &Instr : *FalseBasicBlock) {
        llvm::outs() << "\t [INST" << I++ << "]: " << &Instr << "\n";
      }
    }
  }
  if (FalseBasicBlock != nullptr) {
    jobject LabelNode = Wala->makeConstant(BasicBlockLabeller::label(FalseBasicBlock).c_str());
    FalseGotoNode = Wala->makeNode(CAstWrapper::GOTO, LabelNode);
  }

  // 4. Assemble them into an if-stmt node
  jobject IfStmtNode = Wala->makeNode(CAstWrapper::EMPTY);
  if (FalseGotoNode != nullptr) { // with else block
    IfStmtNode = Wala->makeNode(CAstWrapper::IF_STMT, CondNode, TrueGotoNode, FalseGotoNode);
  } else { // without else block
    IfStmtNode = Wala->makeNode(CAstWrapper::IF_STMT, CondNode, TrueGotoNode);
  }
  NodeMap.insert(std::make_pair(CBI, IfStmtNode));
  return IfStmtNode;
}

jobject SILWalaInstructionVisitor::visitSwitchValueInst(SwitchValueInst *SVI) {

  SILValue Cond = SVI->getOperand();
  jobject CondNode = findAndRemoveCAstNode(Cond.getOpaqueValue());
  
  if (Print) {
    llvm::outs() << "\t [COND]: " << Cond.getOpaqueValue() << "\n";
  }

  // Make children
  list<jobject> Children;

  for (unsigned Idx = 0, Num = SVI->getNumCases(); Idx < Num; ++Idx) {
    auto Case = SVI->getCase(Idx);

    jobject CaseValNode = findAndRemoveCAstNode(Case.first);
    SILBasicBlock *CaseBasicBlock = Case.second;

    Children.push_back(CaseValNode);

    auto LabelNodeName = BasicBlockLabeller::label(CaseBasicBlock);
    jobject LabelNode = Wala->makeConstant(LabelNodeName.c_str());
    Children.push_back(LabelNode);

    if (Print) {
      if (SVI->hasDefault() && CaseBasicBlock == SVI->getDefaultBB()) {
        // Default Node.
        llvm::outs() << "\t [DEFAULT]: " << LabelNode << " => " << *CaseBasicBlock << "\n";
      } else {
        // Not Default Node.
        llvm::outs() << "\t [CASE]: VAL = " << CaseValNode << " " << LabelNodeName << " => " << *CaseBasicBlock << "\n";
      }

      int I = 0;
      for (auto &Instr : *CaseBasicBlock) {
        llvm::outs() << "\t [INST" << I++ << "]: " << &Instr << "\n";
      }
    }

    auto GotoCaseNode = Wala->makeNode(CAstWrapper::GOTO, LabelNode);
    Children.push_back(GotoCaseNode);
  }

  auto SwitchCasesNode = Wala->makeNode(CAstWrapper::BLOCK_STMT,  Wala->makeArray(&Children));
  auto SwitchNode = Wala->makeNode(CAstWrapper::SWITCH, CondNode, SwitchCasesNode);

  NodeMap.insert(std::make_pair(SVI, SwitchCasesNode));

  return SwitchNode;
}

jobject SILWalaInstructionVisitor::visitSelectValueInst(SelectValueInst *SVI) {

  if (Print) {
    llvm::outs() << "\t This should never be reached! Swift does not support this anymore" << "\n";
  }

  return Wala->makeNode(CAstWrapper::EMPTY);
}

jobject SILWalaInstructionVisitor::visitSwitchEnumInst(SwitchEnumInst *SWI) {

  SILValue Cond = SWI->getOperand();
  jobject CondNode = findAndRemoveCAstNode(Cond.getOpaqueValue());

  if (Print) {
    llvm::outs() << "\t [COND]: " << Cond.getOpaqueValue() << "\n";
  }

  list<jobject> Children;

  for (unsigned Idx = 0, Num = SWI->getNumCases(); Idx < Num; ++Idx) {
    auto Case = SWI->getCase(Idx);
    EnumElementDecl *CaseDecl = Case.first;
    SILBasicBlock *CaseBasicBlock = Case.second;

    auto LabelNodeName = BasicBlockLabeller::label(CaseBasicBlock);
    jobject LabelNode = Wala->makeConstant(LabelNodeName.c_str());
    Children.push_back(LabelNode);

    if (Print) {
      if (SWI->hasDefault() && CaseBasicBlock == SWI->getDefaultBB()) {
        // Default Node.
        llvm::outs() << "\t [DEFAULT]: " << LabelNode << " => " << *CaseBasicBlock << "\n";
      } else {
        // Not Default Node.
        llvm::outs() << "\t [CASE]: DECL = " << CaseDecl << " " << LabelNodeName << " => " << *CaseBasicBlock << "\n";
      }

      int I = 0;
      for (auto &Instr : *CaseBasicBlock) {
        llvm::outs() << "\t [INST" << I++ << "]: " << &Instr << "\n";
      }
    }

    auto GotoCaseNode = Wala->makeNode(CAstWrapper::GOTO, LabelNode);
    Children.push_back(GotoCaseNode);
  }

  auto SwitchCasesNode = Wala->makeNode(CAstWrapper::BLOCK_STMT,  Wala->makeArray(&Children));
  auto SwitchNode = Wala->makeNode(CAstWrapper::SWITCH, CondNode, SwitchCasesNode);

  NodeMap.insert(std::make_pair(SWI, SwitchNode));

  return SwitchNode;
}

jobject SILWalaInstructionVisitor::visitSwitchEnumAddrInst(SwitchEnumAddrInst *SEAI) {
  SILValue ConditionOperand = SEAI->getOperand();

  jobject CondNode = findAndRemoveCAstNode(ConditionOperand.getOpaqueValue());

  list<jobject> Children;

  for (unsigned i = 0, e = SEAI->getNumCases(); i < e; ++i) {
      EnumElementDecl *Element;
      SILBasicBlock *DestinationBlock;

      std::tie(Element, DestinationBlock) = SEAI->getCase(i);

      string ElementNameString = Element->getNameStr();
      jobject ElementNameNode = Wala->makeConstant(ElementNameString.c_str());

      if (Print) {
         llvm::outs() << "\t [BASIC BLOCK]: " << DestinationBlock << "\n";
      }

      jobject BlockLabelNodeName = Wala->makeConstant(BasicBlockLabeller::label(DestinationBlock).c_str());
      jobject GotoNode = Wala->makeNode(CAstWrapper::GOTO, BlockLabelNodeName);

      Children.push_back(ElementNameNode);
      Children.push_back(GotoNode);
    }

    if (SEAI->hasDefault()) {
      SILBasicBlock *DestinationBlock = SEAI->getDefaultBB();

      string ElementNameString = "DEFAULT";
      jobject ElementNameNode = Wala->makeConstant(ElementNameString.c_str());

      if (Print) {
         llvm::outs() << "\t [DEFAULT BASIC BLOCK]: " << DestinationBlock << "\n";
      }

      jobject BlockLabelNodeName = Wala->makeConstant(BasicBlockLabeller::label(DestinationBlock).c_str());
      jobject GotoNode = Wala->makeNode(CAstWrapper::GOTO, BlockLabelNodeName);

      Children.push_back(ElementNameNode);
      Children.push_back(GotoNode);
    }

    jobject EnumNode = Wala->makeNode(CAstWrapper::BLOCK_STMT,  Wala->makeArray(&Children));
    jobject SwitchEnumAddrNode = Wala->makeNode(CAstWrapper::SWITCH, CondNode, EnumNode);

    NodeMap.insert(std::make_pair(SEAI, SwitchEnumAddrNode));

    return SwitchEnumAddrNode;
}

jobject SILWalaInstructionVisitor::visitCheckedCastBranchInst(CheckedCastBranchInst *CI) {
  SILValue CastingOperand = CI->getOperand();
  SILType CastType = CI->getCastType();

  if (Print) {
    llvm::outs() << "\t [CONVERT]: " << CastingOperand.getOpaqueValue() << "\n";
    llvm::outs() << "\t [TO]: " << CastType.getAsString() <<  "\n";
  }

  // 1. Cast statement
  jobject CastingNode = findAndRemoveCAstNode(CastingOperand.getOpaqueValue());
  jobject CastingTypeNode =  Wala->makeConstant(CastType.getAsString().c_str());

  jobject ConversionNode = Wala->makeNode(CAstWrapper::CAST, CastingNode, CastingTypeNode);

  // 2. Success block
  SILBasicBlock *SuccessBlock = CI->getSuccessBB();

  if (Print) {
     llvm::outs() << "\t [SUCCESS BASIC BLOCK]: " << SuccessBlock << "\n";
  }

  jobject SuccessBlockNode = Wala->makeConstant(BasicBlockLabeller::label(SuccessBlock).c_str());
  jobject SuccessGoToNode = Wala->makeNode(CAstWrapper::GOTO, SuccessBlockNode);

  // 3. False block
  SILBasicBlock *FailureBlock = CI->getFailureBB();

  if (Print) {
     llvm::outs() << "\t [FAILIURE BASIC BLOCK]: " << FailureBlock << "\n";
  }

  jobject FailureBlockNode = Wala->makeConstant(BasicBlockLabeller::label(FailureBlock).c_str());
  jobject FailureGoToNode = Wala->makeNode(CAstWrapper::GOTO, FailureBlockNode);

  // 4. Assemble them into an if-stmt node
  jobject StmtNode = Wala->makeNode(CAstWrapper::IF_STMT, ConversionNode, SuccessGoToNode, FailureGoToNode);

  NodeMap.insert(std::make_pair(CI, StmtNode));
  return StmtNode;
}

jobject SILWalaInstructionVisitor::visitCheckedCastAddrBranchInst(CheckedCastAddrBranchInst *CI) {
  SILValue SrcValue = CI->getSrc();
  SILValue DestValue = CI->getDest();

  if (Print) {
    llvm::outs() << "\t [CONVERT]: " << CI->getSourceType().getString() << " " << SrcValue.getOpaqueValue();
    llvm::outs() << " [TO]: " << CI->getTargetType().getString() << " " << DestValue.getOpaqueValue() << "\n";
  }

  // 1. Cast statement
  jobject SrcNode = findAndRemoveCAstNode(SrcValue.getOpaqueValue());
  jobject DestNode = findAndRemoveCAstNode(DestValue.getOpaqueValue());

  jobject ConversionNode = Wala->makeNode(CAstWrapper::CAST, SrcNode, DestNode);

  // 2. Success block
  SILBasicBlock *SuccessBlock = CI->getSuccessBB();

  if (Print) {
     llvm::outs() << "\t [SUCCESS BASIC BLOCK]: " << SuccessBlock << "\n";
  }

  jobject SuccessBlockNode = Wala->makeConstant(BasicBlockLabeller::label(SuccessBlock).c_str());
  jobject SuccessGoToNode = Wala->makeNode(CAstWrapper::GOTO, SuccessBlockNode);

  // 3. False block
  SILBasicBlock *FailureBlock = CI->getFailureBB();

  if (Print) {
     llvm::outs() << "\t [FAILIURE BASIC BLOCK]: " << FailureBlock << "\n";
  }

  jobject FailureBlockNode = Wala->makeConstant(BasicBlockLabeller::label(FailureBlock).c_str());
  jobject FailureGoToNode = Wala->makeNode(CAstWrapper::GOTO, FailureBlockNode);

  // 4. Assemble them into an if-stmt node
  jobject StmtNode = Wala->makeNode(CAstWrapper::IF_STMT, ConversionNode, SuccessGoToNode, FailureGoToNode);

  NodeMap.insert(std::make_pair(CI, StmtNode));
  return StmtNode;
}

jobject SILWalaInstructionVisitor::visitTryApplyInst(TryApplyInst *TAI) {
  auto Call = visitApplySite(ApplySite(TAI));
  jobject TryFunc = Wala->makeNode(CAstWrapper::TRY, Call);
  jobject VarName = Wala->makeConstant("resulf_of_try");
  jobject Var = Wala->makeNode(CAstWrapper::VAR, VarName);
  jobject Node = Wala->makeNode(CAstWrapper::ASSIGN, Var, TryFunc);
  NodeMap.insert(std::make_pair(TAI, Node));
  SILBasicBlock *BB = TAI->getNormalBB();
  SymbolTable.insert(BB->getArgument(0), "resulf_of_try"); // insert the node into the hash map
  return Node;
}

}
