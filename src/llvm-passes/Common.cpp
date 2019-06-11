#include "Common.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"

using namespace std;
using namespace llvm;
using namespace errspec;

namespace errspec {

string getCalleeName(const CallInst &I) {
  string fname = "";
  const Value *callee = I.getCalledValue();
  if (!callee)
    return fname;
  fname = callee->stripPointerCasts()->getName();
  return fname;
}

llvm::Instruction *GetFirstInstructionOfBB(llvm::BasicBlock *bb) {
  return &(*bb->begin());
}

llvm::Instruction *GetLastInstructionOfBB(llvm::BasicBlock *bb) {
  llvm::Instruction *bb_last;
  for (auto ii = bb->begin(), ie = bb->end(); ii != ie; ++ii) {
    bb_last = &(*ii);
  }
  return bb_last;
}
}