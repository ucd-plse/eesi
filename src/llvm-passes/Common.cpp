#include "Constraint.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"

using namespace std;
using namespace llvm;

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

pair<Interval, Interval> abstractICmp(ICmpInst &I) {
  Interval true_interval = Interval::TOP;
  Interval false_interval = Interval::TOP;

  ConstantInt *num = dyn_cast<ConstantInt>(I.getOperand(1));
  if (!num) {
    num = dyn_cast<ConstantInt>(I.getOperand(0));
  }

  // icmp tests against zero or null
  if ((num && num->isZero()) || isa<ConstantPointerNull>(I.getOperand(1))) {
    ICmpInst::Predicate predicate = I.getSignedPredicate();
    if (predicate == ICmpInst::Predicate::ICMP_SLE) {
      true_interval = Interval::LEZ;
      false_interval = Interval::GTZ;
    } else if (predicate == ICmpInst::Predicate::ICMP_SLT) {
      true_interval = Interval::LTZ;
      false_interval = Interval::GEZ;
    } else if (predicate == ICmpInst::Predicate::ICMP_SGT) {
      true_interval = Interval::GTZ;
      false_interval = Interval::LEZ;
    } else if (predicate == ICmpInst::Predicate::ICMP_SGE) {
      true_interval = Interval::GEZ;
      false_interval = Interval::LTZ;
    } else if (predicate == ICmpInst::Predicate::ICMP_EQ) {
      true_interval = Interval::ZERO;
      false_interval = Interval::NTZ;
    } else if (predicate == ICmpInst::Predicate::ICMP_NE) {
      true_interval = Interval::NTZ;
      false_interval = Interval::ZERO;
    } else {
      true_interval = Interval::TOP;
      false_interval = Interval::TOP;
    }

    // These negations go to TOP because we abstract individual
    // negative integers to less than zero. And the negation
    // of a single negative integer might still be negative.
  } else if (num && num->isNegative()) {
    true_interval = Interval::LTZ;
    false_interval = Interval::TOP;
  } else if (num && !num->isNegative()) {
    true_interval = Interval::GTZ;
    false_interval = Interval::TOP;
  }

  return make_pair(true_interval, false_interval);
}



}