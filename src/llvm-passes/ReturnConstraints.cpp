#include <fstream>
#include <iostream>
#include <string>

#include "Common.h"
#include "ReturnConstraints.h"
#include "ReturnPropagation.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"

using namespace llvm;
using namespace std;
using namespace errspec;

#define DEBUG false

bool ReturnConstraints::runOnModule(Module &M) {
  // Initialize program points to empty ReturnConstraintsFact
  // Creates a new fact at every point
  std::shared_ptr<ReturnConstraintsFact> prev = nullptr;
  for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    for (auto bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      for (auto ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
        Instruction *inst = &(*ii);
        if (prev == nullptr) {
          input_facts[inst] = std::make_shared<ReturnConstraintsFact>();
        } else {
          input_facts[inst] = prev;
        }
        auto out = std::make_shared<ReturnConstraintsFact>();
        output_facts[inst] = out;
        prev = out;
      }
      prev = nullptr;
    }
  }

  for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    runOnFunction(*fi);
  }

  return false;
}

void ReturnConstraints::runOnFunction(Function &F) {
  string fname = F.getName().str();

  bool changed = true;
  while (changed) {
    changed = false;

    for (auto bi = F.begin(), be = F.end(); bi != be; ++bi) {
      BasicBlock *BB = &*bi;
      Instruction *succ_begin = &*(BB->begin());
      auto succ_fact = input_facts.at(succ_begin);

      // Go over predecessor blocks and apply join
      for (auto pi = pred_begin(BB), pe = pred_end(BB); pi != pe; ++pi) {
        Instruction *pred_term = (*pi)->getTerminator();
        auto pred_fact = output_facts.at(pred_term);
        succ_fact->join(*pred_fact);
      }

      changed = visitBlock(*BB) || changed;
    }

    if (DEBUG) {
      for (auto bi = F.begin(), be = F.end(); bi != be; ++bi) {
        for (auto ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
          input_facts.at(&*ii)->dump();
          ii->dump();
          output_facts.at(&*ii)->dump();
        }
      }
    }
  }
  return;
}

bool ReturnConstraints::visitBlock(BasicBlock &BB) {
  bool changed = false;
  for (auto ii = BB.begin(), ie = BB.end(); ii != ie; ++ii) {
    Instruction &I = *ii;

    shared_ptr<ReturnConstraintsFact> input_fact = input_facts.at(&I);
    shared_ptr<ReturnConstraintsFact> output_fact = output_facts.at(&I);

    ReturnConstraintsFact prev_fact = *output_fact;
    if (CallInst *inst = dyn_cast<CallInst>(&I)) {
      visitCallInst(*inst, input_fact, output_fact);
    } else if (BranchInst *inst = dyn_cast<BranchInst>(&I)) {
      visitBranchInst(*inst, input_fact, output_fact);
    } else if (PHINode *inst = dyn_cast<PHINode>(&I)) {
      visitPHINode(*inst, input_fact, output_fact);
    } else {
      // Default is to just copy facts from previous instruction unchanged.
      output_fact->value = input_fact->value;
    }
    changed = changed || (*(output_fact) != prev_fact);
  }

  return changed;
}

void ReturnConstraints::visitCallInst(
    CallInst &I, shared_ptr<const ReturnConstraintsFact> in,
    shared_ptr<ReturnConstraintsFact> out) {
  out->value = in->value;
  unordered_set<Value *> gen_value({&I});

  string callee_name = getCalleeName(I);

  Constraint c(callee_name);
  c.interval = Interval::TOP;

  out->value[callee_name] = c;
}

pair<Interval, Interval> ReturnConstraints::abstractICmp(ICmpInst &I) {
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

void ReturnConstraints::visitBranchInst(
    BranchInst &I, shared_ptr<const ReturnConstraintsFact> in,
    shared_ptr<ReturnConstraintsFact> out) {
  out->value = in->value;

  if (I.isUnconditional()) {
    return;
  }
  Value *condition = I.getOperand(0);
  assert(condition);

  // TODO: If it is a call to IS_ERR we should handle it too.
  ICmpInst *icmp = dyn_cast<ICmpInst>(condition);
  if (!icmp) {
    return;
  }

  ReturnPropagation *return_propagation = &getAnalysis<ReturnPropagation>();

  BasicBlock *true_bb = dyn_cast<BasicBlock>(I.getOperand(2));
  assert(true_bb);
  BasicBlock *false_bb = dyn_cast<BasicBlock>(I.getOperand(1));
  assert(false_bb);
  Interval true_abstract_value = abstractICmp(*icmp).first;
  Interval false_abstract_value = abstractICmp(*icmp).second;

  // Get the set of function whose values reach icmp operand from
  // return-propagation
  Value *icmp_value = icmp->getOperand(0);
  if (return_propagation->output_facts.find(icmp_value) ==
      return_propagation->output_facts.end()) {
    return;
  }

  auto fact = return_propagation->output_facts.at(icmp_value);

  // The first element of this pair is the llvm value being tested
  // The second element is the set of functions which the key value may hold.
  unordered_set<Value *> test_ret_values;
  for (auto element : fact->value) {
    if (element.first == icmp_value) {
      test_ret_values = element.second;
    }
  }

  for (Value *v : test_ret_values) {
    ReturnConstraintsFact true_fact;
    ReturnConstraintsFact false_fact;

    if (!isa<CallInst>(v))
      continue;

    // Get the function name associated with v
    CallInst *call = dyn_cast<CallInst>(v);
    string fname = getCalleeName(*call);

    // kill constraints for functions being tested
    // prevents predecessor join from setting everything to top
    // splits constraint interval at branches
    Constraint kill_constraint(fname);
    kill_constraint.interval = Interval::BOT;
    out->value[fname] = kill_constraint;

    Constraint true_c(fname);
    true_c.interval = true_abstract_value;
    Constraint false_c(fname);
    false_c.interval = false_abstract_value;

    true_fact.value[fname] = true_c;
    false_fact.value[fname] = false_c;

    Instruction *true_first = GetFirstInstructionOfBB(true_bb);
    auto existing_true_fact = input_facts.at(true_first);
    existing_true_fact->meet(true_fact);

    Instruction *false_first = GetFirstInstructionOfBB(false_bb);
    auto existing_false_fact = input_facts.at(false_first);
    existing_false_fact->meet(false_fact);
  }
}

// If the PHI result can be returned, then add incoming values
// to the exit of each incoming basic block
void ReturnConstraints::visitPHINode(PHINode &I,
                                     shared_ptr<const ReturnConstraintsFact> in,
                                     shared_ptr<ReturnConstraintsFact> out) {

  out->value = in->value;
}

ReturnConstraintsFact ReturnConstraints::getInFact(Value *v) const {
  return *(input_facts.at(v));
}

ReturnConstraintsFact ReturnConstraints::getOutFact(Value *v) const {
  return *(output_facts.at(v));
}

void ReturnConstraints::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<ReturnPropagation>();
  AU.setPreservesAll();
}

char ReturnConstraints::ID = 0;
static RegisterPass<ReturnConstraints>
    X("return-constraints",
      "Function Return Value Constraints on each basic block", false, false);
