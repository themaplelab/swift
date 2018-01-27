#include "swift/SIL/SILVisitor.h"
#include "swift/WALASupport/WALAWalker.h"

#include "jni.h"
#include <unordered_map>

#ifndef SWIFT_SILWALAINSTRUCTIONVISITOR_H
#define SWIFT_SILWALAINSTRUCTIONVISITOR_H

namespace swift {

class SILWalaInstructionVisitor : public SILInstructionVisitor<SILWalaInstructionVisitor, jobject> {
public:
  SILWalaInstructionVisitor(const WALAIntegration &Wala, bool Print) : Print(Print), Wala(Wala) {}

  void visitSILModule(SILModule *M);
  void visitSILFunction(SILFunction *F);
  void visitSILBasicBlock(SILBasicBlock *BB);
  void visitModule(SILModule *M);
  void beforeVisit(SILInstruction *I);

  jobject visitSILInstruction(SILInstruction *I) { return nullptr; }

  jobject visitApplyInst(ApplyInst *AI);

private:
  void updateInstrSourceInfo(SILInstruction *I);
  void perInstruction();
  jobject findAndRemoveCAstNode(void* Key);

public:
  unsigned int InstructionCount = 0;

private:
  bool Print;

  WALAIntegration Wala;
  SymbolTable SymbolTable;
  std::shared_ptr<WALAWalker::InstrInfo> InstrInfo;
  std::shared_ptr<WALAWalker::FunctionInfo> FunctionInfo;
  std::shared_ptr<WALAWalker::ModuleInfo> ModuleInfo;

  std::unordered_map<void *, jobject> NodeMap;
  std::list<jobject> NodeList;
  std::list<jobject> BlockStmtList;
};

}

#endif //SWIFT_SILWALAINSTRUCTIONVISITOR_H
