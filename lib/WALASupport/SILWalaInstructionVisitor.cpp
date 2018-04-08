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

jobject SILWalaInstructionVisitor::visitApplyInst(ApplyInst *AI) {
  if (auto Node = visitApplySite(AI)) {
    NodeMap.insert(std::make_pair(AI, Node)); // insert the node into the hash map
    return Node;
  }
  return nullptr;
}

jobject SILWalaInstructionVisitor::visitAllocBoxInst(AllocBoxInst *ABI) {
  SILDebugVariable Info = ABI->getVarInfo();
  unsigned ArgNo = Info.ArgNo;

  if (auto *Decl = ABI->getDecl()) {
    StringRef varName = Decl->getNameStr();
    if (Print) {
      llvm::outs() << "[Arg]#" << ArgNo << ":" << varName << "\n";
    }
    SymbolTable.insert(ABI, varName);
  }
  else {
    // temporary allocation when referencing self.
    if (Print) {
      llvm::outs() << "[Arg]#" << ArgNo << ":" << "self" << "\n";
    }
    SymbolTable.insert(ABI, "self");
  }
  return nullptr;
}

jobject SILWalaInstructionVisitor::visitIntegerLiteralInst(IntegerLiteralInst *ILI) {
  APInt Value = ILI->getValue();
  jobject Node = nullptr;
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
    NodeMap.insert(std::make_pair(ILI, Node));
  }
  return Node;
}

jobject SILWalaInstructionVisitor::visitFloatLiteralInst(FloatLiteralInst *FLI) {
  if (Print) {
    llvm::outs() << "<< FloatLiteralInst >>" << "\n";
  }
  jobject Node = nullptr;
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
  NodeMap.insert(std::make_pair(FLI, Node));
  return Node;
}

jobject SILWalaInstructionVisitor::visitStringLiteralInst(StringLiteralInst *SLI) {
  // Value: the string data for the literal, in UTF-8.
  StringRef Value = SLI->getValue();

  if (Print) {
    llvm::outs() << "\t [value] " << Value << "\n";
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
    llvm::outs() << "\t [encoding] " << encoding << "\n";
  }

  // Count: encoding-based length of the string literal in code units.
  uint64_t codeUnitCount = SLI->getCodeUnitCount();

  if (Print) {
    llvm::outs() << "\t [codeUnitCount] " << codeUnitCount << "\n";
  }

  // Call WALA in Java
  jobject walaConstant = Wala->makeConstant((Value.str()).c_str());
  NodeMap.insert(std::make_pair(SLI, walaConstant));
  return walaConstant;
}

jobject SILWalaInstructionVisitor::visitConstStringLiteralInst(ConstStringLiteralInst *CSLI) {
  // Value: the string data for the literal, in UTF-8.
  StringRef Value = CSLI->getValue();
  if (Print) {
    llvm::outs() << "\t [value] " << Value << "\n";
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

  NodeMap.insert(std::make_pair(CSLI, C));

  return C;

}

jobject SILWalaInstructionVisitor::visitProjectBoxInst(ProjectBoxInst *PBI) {
  if (SymbolTable.has(PBI->getOperand().getOpaqueValue())) {
    // this is a variable
    SymbolTable.duplicate(PBI, SymbolTable.get(PBI->getOperand().getOpaqueValue()).c_str());
  }
  return nullptr;
}

jobject SILWalaInstructionVisitor::visitDebugValueInst(DebugValueInst *DBI) {
  SILDebugVariable Info = DBI->getVarInfo();
  unsigned ArgNo = Info.ArgNo;

  if (Print) {
    llvm::outs() << "<< DebugValueInst >>" << "\n";
    llvm::outs() << "argNo: " << ArgNo << "\n";
  }

  VarDecl *Decl = DBI->getDecl();

  if (Decl) {
    string VarName = Decl->getNameStr();
    if (VarName.length() == 0) {
      llvm::outs() << "\t DebugValue empty name \n";
      return nullptr;
    }
    SILValue Val = DBI->getOperand();
    if (!Val) {
      if (Print) {
        llvm::outs() << "\t Operand is null\n";
      }
      return nullptr;
    }

    void *Addr = Val.getOpaqueValue();
    if (Addr) {
      SymbolTable.insert(Addr, VarName);
      llvm::outs() << "\t[addr of arg]:" << Addr << "\n";
    }
    else {
      if (Print) {
        llvm::outs() << "\t Operand OpaqueValue is null\n";
      }
      return nullptr;
    }
  }
  else {
    if (Print) {
      llvm::outs() << "\tDecl not found\n";
    }
  }

  return nullptr;
}

jobject SILWalaInstructionVisitor::visitFunctionRefInst(FunctionRefInst *FRI) {
  // Cast the instr to access methods
  string FuncName = Demangle::demangleSymbolAsString(FRI->getReferencedFunction()->getName());
  jobject NameNode = Wala->makeConstant(FuncName.c_str());
  jobject FuncExprNode = Wala->makeNode(CAstWrapper::FUNCTION_EXPR, NameNode);

  if (Print) {
    llvm::outs() << "=== [FUNC] Ref'd: ";
    llvm::outs() << FuncName << "\n";
  }

  NodeMap.insert(std::make_pair(FRI->getReferencedFunction(), FuncExprNode));
  NodeMap.insert(std::make_pair(FRI, FuncExprNode));
  return nullptr;
}

jobject SILWalaInstructionVisitor::visitLoadInst(LoadInst *LI) {
  if (Print) {
    llvm::outs() << "\t\t [name]:" << (LI->getOperand()).getOpaqueValue() << "\n";
  }
  jobject Node = findAndRemoveCAstNode((LI->getOperand()).getOpaqueValue());

  NodeMap.insert(std::make_pair(LI, Node));
  return Node;
}

jobject SILWalaInstructionVisitor::visitLoadBorrowInst(LoadBorrowInst *LBI) {
  if (Print) {
    llvm::outs() << "\t\t [name]:" << LBI->getOperand() << "\n";
    llvm::outs() << "\t\t [addr]:" << LBI->getOperand().getOpaqueValue() << "\n";
  }
  return nullptr;
}

jobject SILWalaInstructionVisitor::visitBeginBorrowInst(BeginBorrowInst *BBI) {
  jobject Node = findAndRemoveCAstNode(BBI->getOperand().getOpaqueValue());
  NodeMap.insert(std::make_pair(BBI, Node));
  return Node;
}

jobject SILWalaInstructionVisitor::visitThinToThickFunctionInst(ThinToThickFunctionInst *TTFI) {
  // Cast the instr to access methods
  if (Print) {
    llvm::outs() << "Callee: ";
    llvm::outs() << TTFI->getCallee().getOpaqueValue() << "\n";
  }
  jobject FuncRefNode = findAndRemoveCAstNode(TTFI->getCallee().getOpaqueValue());
  // cast in CASt
  NodeMap.insert(std::make_pair(TTFI, FuncRefNode));
  return nullptr;
}

jobject SILWalaInstructionVisitor::visitStoreInst(StoreInst *SI) {
  // Cast the instr to access methods
  SILValue Src = SI->getSrc();
  SILValue Dest = SI->getDest();
  if (Print) {
    char Str[80];
    llvm::outs() << "\t [SRC]: " << Src.getOpaqueValue() << "\n";
    sprintf(Str, "instrKind: %d\n", Src->getKind());
    llvm::outs() << Str;
    llvm::outs() << "\t [DEST]: " << Dest.getOpaqueValue() << "\n";
    llvm::outs() << Str;
  }

  jobject Node = nullptr;
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

jobject SILWalaInstructionVisitor::visitBeginAccessInst(BeginAccessInst *BAI) {
  if (Print) {
    llvm::outs() << "\t\t [oper_addr]:" << (BAI->getSource()).getOpaqueValue() << "\n";
  }
  jobject Var = findAndRemoveCAstNode(BAI->getSource().getOpaqueValue());
  NodeMap.insert(std::make_pair(BAI, Var));
  return nullptr;
}

jobject SILWalaInstructionVisitor::visitAssignInst(AssignInst *AI) {
  if (Print) {
    llvm::outs() << "[source]:" << AI->getSrc().getOpaqueValue() << "\n";
    llvm::outs() << "[Dest]:" << AI->getDest().getOpaqueValue() << "\n";
  }
  jobject Dest = findAndRemoveCAstNode(AI->getDest().getOpaqueValue());
  jobject Expr = findAndRemoveCAstNode(AI->getSrc().getOpaqueValue());

  jobject Node = Wala->makeNode(CAstWrapper::ASSIGN, Dest, Expr);
  NodeMap.insert(std::make_pair(AI, Node));
  return Node;
}

jobject SILWalaInstructionVisitor::visitAllocStackInst(AllocStackInst *ASI) {
  if (Print) {
    for (auto &OP : ASI->getAllOperands()) {
      llvm::outs() << "\t [OPERAND]: " << OP.get() << "\n";
      llvm::outs() << "\t [ADDR]: " << OP.get().getOpaqueValue() << "\n";
    }
  }
  return nullptr;
}

jobject SILWalaInstructionVisitor::visitReturnInst(ReturnInst *RI) {
  SILValue RV = RI->getOperand();

  if (Print) {
    llvm::outs() << "operand:" << RV << "\n";
    llvm::outs() << "addr:" << RV.getOpaqueValue() << "\n";
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
    llvm::outs() << "operand:" << TV << "\n";
    llvm::outs() << "addr:" << TV.getOpaqueValue() << "\n";
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
    NodeMap.insert(std::make_pair(TV, Node));
  }
  return Node;
}

jobject SILWalaInstructionVisitor::visitBranchInst(BranchInst *BI) {
  // This is an unconditional branch
  jobject GotoNode = nullptr;

  // Destination block
  int I = 0;
  SILBasicBlock *Dest = BI->getDestBB();
  if (Print) {
    llvm::outs() << "\t [DESTBB]: " << Dest << "\n";
    if (Dest != nullptr) {
      for (auto &Instr : *Dest) {
        llvm::outs() << "\t\t [INST" << I++ << "]: " << &Instr << "\n";
      }
    }
  }
  if (Dest != nullptr) {
    jobject LabelNode = Wala->makeConstant(BasicBlockLabeller::label(Dest).c_str());
    GotoNode = Wala->makeNode(CAstWrapper::GOTO, LabelNode);
  }

  for (unsigned Idx = 0; Idx < BI->getNumArgs(); Idx++) {
    llvm::outs() << "addr:" << Dest->getArgument(Idx) << "\n";
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
  jobject TrueGotoNode = nullptr;
  if (Print) {
    llvm::outs() << "\t [TBB]: " << TrueBasicBlock << "\n";
    if (TrueBasicBlock != nullptr) {
      for (auto &Instr : *TrueBasicBlock) {
        llvm::outs() << "\t\t [INST" << I++ << "]: " << &Instr << "\n";
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
  jobject FalseGotoNode = nullptr;
  if (Print) {
    llvm::outs() << "\t [FBB]: " << FalseBasicBlock << "\n";
    if (FalseBasicBlock != nullptr) {
      for (auto &Instr : *FalseBasicBlock) {
        llvm::outs() << "\t\t [INST" << I++ << "]: " << &Instr << "\n";
      }
    }
  }
  if (FalseBasicBlock != nullptr) {
    jobject LabelNode = Wala->makeConstant(BasicBlockLabeller::label(FalseBasicBlock).c_str());
    FalseGotoNode = Wala->makeNode(CAstWrapper::GOTO, LabelNode);
  }

  // 4. Assemble them into an if-stmt node
  jobject IfStmtNode = nullptr;
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

  SILBasicBlock *FalseBasicBlock = nullptr;
  SILBasicBlock *TrueBasicBlock = nullptr;
  
  // Evaluate cases
  for (unsigned Idx = 0, Num = SVI->getNumCases(); Idx < Num; ++Idx) {
    auto Case = SVI->getCase(Idx);
    auto *CaseVal = dyn_cast<IntegerLiteralInst>(Case.first);

    if (!CaseVal)
      return nullptr;
    SILBasicBlock *DestBasicBlock = Case.second;
    if (CaseVal->getValue() == 0) {
      FalseBasicBlock = DestBasicBlock;
    } else {
      TrueBasicBlock = DestBasicBlock;
    }
  }

  // Check for defaults
  if (SVI->hasDefault()) {
    if (!FalseBasicBlock) {
      FalseBasicBlock = SVI->getDefaultBB();
    } else if (!TrueBasicBlock) {
      TrueBasicBlock = SVI->getDefaultBB();
    }
  }

  if (!FalseBasicBlock || !TrueBasicBlock)
    return nullptr;


  int I = 0;
  jobject TrueGotoNode = nullptr;
  if (Print) {
    llvm::outs() << "\t [TBB]: " << TrueBasicBlock << "\n";
    if (TrueBasicBlock != nullptr) {
      for (auto &Instr : *TrueBasicBlock) {
        llvm::outs() << "\t\t [INST" << I++ << "]: " << &Instr << "\n";
      }
    }
  }

  if (TrueBasicBlock != nullptr) {
    jobject LabelNode = Wala->makeConstant(BasicBlockLabeller::label(TrueBasicBlock).c_str());
    TrueGotoNode = Wala->makeNode(CAstWrapper::GOTO, LabelNode);
  }
  
  I = 0;
  jobject FalseGotoNode = nullptr;
  if (Print) {
    llvm::outs() << "\t [FBB]: " << FalseBasicBlock << "\n";
    if (FalseBasicBlock != nullptr) {
      for (auto &Instr : *FalseBasicBlock) {
        llvm::outs() << "\t\t [INST" << I++ << "]: " << &Instr << "\n";
      }
    }
  }

  FalseGotoNode = nullptr;
  if (FalseBasicBlock != nullptr) {
    jobject LabelNode = Wala->makeConstant(BasicBlockLabeller::label(FalseBasicBlock).c_str());
    FalseGotoNode = Wala->makeNode(CAstWrapper::GOTO, LabelNode);
  }

  jobject IfStmtNode = nullptr;
  if (FalseGotoNode != nullptr) { // with else block
    IfStmtNode = Wala->makeNode(CAstWrapper::IF_STMT, CondNode, TrueGotoNode, FalseGotoNode);
  } else { // without else block
    IfStmtNode = Wala->makeNode(CAstWrapper::IF_STMT, CondNode, TrueGotoNode);
  }
  NodeMap.insert(std::make_pair(SVI, IfStmtNode));

  return IfStmtNode;

}

jobject SILWalaInstructionVisitor::visitEnumInst(EnumInst *EI) {
 
  list<jobject> Properties;

  StringRef discriminantName = EI->getElement()->getNameStr();

  jobject DiscriminantNameNode = Wala->makeConstant("__DISCRIMINATOR__");
  jobject DiscriminantValueNode = Wala->makeConstant(discriminantName.data());

  Properties.push_back(DiscriminantNameNode);
  Properties.push_back(DiscriminantValueNode);

  if (Print) {
    llvm::outs() << "[DISCRIMINATOR] " << discriminantName <<  "\n";
  }

  for (Operand &EnumOperand : EI->getElement()->getParentEnum()) {

      unsigned OperandNumberString = EnumOperand.getOperandNumber();

      jobject OperandValueNode = findAndRemoveCAstNode(EnumOperand.get().getOpaqueValue());
      jobject OperandNameNode = Wala->makeConstant(std::to_string(OperandNumberString));

      if (Print) {
        llvm::outs() << "Operand: " << OperandNumberString << " " << OperandValueNode << "\n";
      }

      Properties.push_back(OperandNameNode);
      Properties.push_back(OperandValueNode);
  }

  auto VisitEnumNode = Wala->makeNode(CAstWrapper::OBJECT_LITERAL, Wala->makeArray(&Properties));

  NodeMap.insert(std::make_pair(EI, VisitEnumNode));

  return VisitEnumNode;

 }

jobject SILWalaInstructionVisitor::visitUnreachableInst(UnreachableInst *UI) {
  if (Print) {
    if (UI->isBranch()) {
      llvm::outs() << "This is a terminator of branch!" << "\n";
    }
    if (UI->isFunctionExiting()) {
      llvm::outs() << "This is a terminator of function!" << "\n";
    }
  }
  return nullptr;
}

jobject SILWalaInstructionVisitor::visitCopyValueInst(CopyValueInst *CVI) {
  llvm::outs() << "\t\t [name]:" << CVI->getOperand() << "\n";
  jobject Node = findAndRemoveCAstNode(CVI->getOperand().getOpaqueValue());

  NodeMap.insert(std::make_pair(CVI, Node));
  return Node;
}

jobject SILWalaInstructionVisitor::visitAllocGlobalInst(AllocGlobalInst *AGI) {
  SILGlobalVariable *Var = AGI->getReferencedGlobal();
  StringRef Name = Var->getName();
  SILType Type = Var->getLoweredType();
  if (Print) {
    llvm::outs() << "\t\t[Var name]:" << Name << "\n";
    llvm::outs() << "\t\t[Var type]:" << Type << "\n";
  }
  return nullptr;
}

jobject SILWalaInstructionVisitor::visitGlobalAddrInst(GlobalAddrInst *GAI) {
  SILGlobalVariable *variable = GAI->getReferencedGlobal();
  StringRef Name = variable->getName();
  if (Print) {
    llvm::outs() << "\t\t[Var name]:" << Name << "\n";
  }
  SymbolTable.insert(GAI, Name);
  return nullptr;
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

jobject SILWalaInstructionVisitor::visitBeginApplyInst(BeginApplyInst *BAI) {
  if (auto Node = visitApplySite(BAI)) {
    NodeMap.insert(std::make_pair(BAI, Node)); // insert the node into the hash map
    return Node;
  }
  return nullptr;
}

jobject SILWalaInstructionVisitor::visitPartialApplyInst(PartialApplyInst *PAI) {
  if (auto Node = visitApplySite(PAI)) {
    NodeMap.insert(std::make_pair(PAI, Node)); // insert the node into the hash map
    return Node;
  }
  return nullptr;
}

jobject SILWalaInstructionVisitor::visitApplySite(ApplySite Apply) {
  jobject Node = nullptr; // the CAst node to be created
  auto *Callee = Apply.getReferencedFunction();

  if (!Callee) {
    return nullptr;
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
        else return nullptr;
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

}
