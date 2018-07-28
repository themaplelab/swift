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

  jobject visitSILInstruction(SILInstruction *I) {
    llvm::outs() << "Not handled instruction: \n" << *I << "\n";
    return nullptr;
  }
  
  /*******************************************************************************/
  /*                      Allocation and Deallocation                            */
  /*******************************************************************************/

  jobject visitAllocStackInst(AllocStackInst *ASI);
  jobject visitAllocBoxInst(AllocBoxInst *ABI);
  jobject visitDeallocStackInst(DeallocStackInst *DSI);
  jobject visitDeallocBoxInst(DeallocBoxInst *DBI);
  jobject visitAllocGlobalInst(AllocGlobalInst *AGI);
  jobject visitProjectBoxInst(ProjectBoxInst *PBI);
  jobject visitBeginUnpairedAccessInst(BeginUnpairedAccessInst *BUI);
  jobject visitEndUnpairedAccessInst(EndUnpairedAccessInst *EUAI);

  /*******************************************************************************/
  /*                        Debug Information                                    */
  /*******************************************************************************/

  jobject visitDebugValueInst(DebugValueInst *DBI);
  jobject visitDebugValueAddrInst(DebugValueAddrInst *DVAI);

  /*******************************************************************************/
  /*                        Accessing Memory                                     */
  /*******************************************************************************/

  jobject visitLoadInst(LoadInst *LI);
  jobject visitStoreInst(StoreInst *SI);
  jobject visitBeginBorrowInst(BeginBorrowInst *BBI);
  jobject visitLoadBorrowInst(LoadBorrowInst *LBI);
  jobject visitEndBorrowInst(EndBorrowInst *EBI);
  jobject visitAssignInst(AssignInst *AI);
  jobject visitCopyAddrInst(CopyAddrInst *CAI);
  jobject visitIndexAddrInst(IndexAddrInst *IAI);
  jobject visitBeginAccessInst(BeginAccessInst *BAI);
  jobject visitEndAccessInst(EndAccessInst *EAI);

  /*******************************************************************************/
  /*                        Reference Counting                                   */
  /*******************************************************************************/

  /*******************************************************************************/
  /*                         Literals                                            */
  /*******************************************************************************/

  jobject visitFunctionRefInst(FunctionRefInst *FRI);
  jobject visitGlobalAddrInst(GlobalAddrInst *GAI);
  jobject visitIntegerLiteralInst(IntegerLiteralInst *ILI);
  jobject visitFloatLiteralInst(FloatLiteralInst *FLI);
  jobject visitStringLiteralInst(StringLiteralInst *SLI);
  jobject visitConstStringLiteralInst(ConstStringLiteralInst *CSLI);

  /*******************************************************************************/
  /*                         Dynamic Dispatch                                    */
  /*******************************************************************************/

  jobject visitWitnessMethodInst(WitnessMethodInst *WMI);

  /*******************************************************************************/
  /*                         Function Application                                */
  /*******************************************************************************/

  jobject visitApplyInst(ApplyInst *AI);
  jobject visitBeginApplyInst(BeginApplyInst *BAI);
  jobject visitEndApplyInst(EndApplyInst *EAI);
  jobject visitAbortApplyInst(AbortApplyInst *AAI);
  jobject visitPartialApplyInst(PartialApplyInst *PAI);
  jobject visitBuiltinInst(BuiltinInst *BI);

  /*******************************************************************************/
  /*                          Metatypes                                          */
  /*******************************************************************************/

  jobject visitMetatypeInst(MetatypeInst *MI);
  jobject visitValueMetatypeInst(ValueMetatypeInst *VMI);

  /*******************************************************************************/
  /*                          Aggregate Types                                    */
  /*******************************************************************************/

  jobject visitCopyValueInst(CopyValueInst *CVI);
  jobject visitTupleInst(TupleInst *TI);
  jobject visitTupleExtractInst(TupleExtractInst *TEI);
  jobject visitStructInst(StructInst *SI);
  jobject visitStructExtractInst(StructExtractInst *SEI);
  jobject visitStructElementAddrInst(StructElementAddrInst *SEAI);
  jobject visitRefElementAddrInst(RefElementAddrInst *REAI);

  /*******************************************************************************/
  /*                          Enums                                              */
  /*******************************************************************************/

  jobject visitEnumInst(EnumInst *EI);
  jobject visitUncheckedEnumDataInst(UncheckedEnumDataInst *UED);
  jobject visitSelectEnumInst(SelectEnumInst *SEI);

  /*******************************************************************************/
  /*                          Protocol and Protocol Composition Types            */
  /*******************************************************************************/

  jobject visitInitExistentialAddrInst(InitExistentialAddrInst *IEAI);
  jobject visitDeinitExistentialAddrInst(DeinitExistentialAddrInst *DEAI);
  jobject visitInitExistentialValueInst(InitExistentialValueInst *IEVI);
  jobject visitDeinitExistentialValueInst(DeinitExistentialValueInst *DEVI);
  jobject visitAllocExistentialBoxInst(AllocExistentialBoxInst *AEBI);
  jobject visitProjectExistentialBoxInst(ProjectExistentialBoxInst *PEBI);

  /*******************************************************************************/
  /*                          Blocks                                             */
  /*******************************************************************************/

  /*******************************************************************************/
  /*                          Unchecked Conversions                              */
  /*******************************************************************************/

  jobject visitAddressToPointerInst(AddressToPointerInst *ATPI);
  jobject visitUncheckedRefCastInst(UncheckedRefCastInst *URCI);
  jobject visitThinToThickFunctionInst(ThinToThickFunctionInst *TTFI);
  jobject visitThinFunctionToPointerInst(ThinFunctionToPointerInst *TFPI);
  jobject visitConvertFunctionInst(ConvertFunctionInst *CFI);
  jobject visitUncheckedOwnershipConversionInst(UncheckedOwnershipConversionInst *UOCI);

  /*******************************************************************************/
  /*                          Checked Conversions                                */
  /*******************************************************************************/

  /*******************************************************************************/
  /*                          Runtime Failures                                   */
  /*******************************************************************************/

  /*******************************************************************************/
  /*                           Terminators                                       */
  /*******************************************************************************/

  jobject visitUnreachableInst(UnreachableInst *UI);
  jobject visitReturnInst(ReturnInst *RI);
  jobject visitThrowInst(ThrowInst *TI);
  jobject visitBranchInst(BranchInst *BI);
  jobject visitCondBranchInst(CondBranchInst *CBI);
  jobject visitSwitchValueInst(SwitchValueInst *SVI);
  jobject visitSelectValueInst(SelectValueInst *SVI);
  jobject visitSwitchEnumInst(SwitchEnumInst *SWI);
  jobject visitTryApplyInst(TryApplyInst *TAI);

private:
  void updateInstrSourceInfo(SILInstruction *I);
  void perInstruction();
  jobject visitApplySite(ApplySite Apply);
  jobject findAndRemoveCAstNode(void *Key);
  jobject getOperatorCAstType(Identifier Name);

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
