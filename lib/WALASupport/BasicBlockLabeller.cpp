#include "swift/WALASupport/BasicBlockLabeller.h"

using namespace swift;

string BasicBlockLabeller::label(SILBasicBlock* basicBlock) {
  return (string("BLOCK #") + std::to_string(basicBlock->getDebugID()));
}
