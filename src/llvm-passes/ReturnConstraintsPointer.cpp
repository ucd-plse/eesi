#include <fstream>
#include <iostream>
#include <string>

#include "Common.h"
#include "ReturnConstraintsPointer.h"
#include "ReturnPropagationPointer.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"

using namespace llvm;
using namespace std;
using namespace errspec;

#define DEBUG false

bool ReturnConstraintsPointer::runOnModule(Module &M) {
  // Initialize program points to empty ReturnConstraintsPointerFact
  // Creates a new fact at every point
  std::shared_ptr<ReturnConstraintsPointerFact> prev = nullptr;
  for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    for (auto bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      for (auto ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
        Instruction *inst = &(*ii);
        if (prev == nullptr) {
          input_facts[inst] = std::make_shared<ReturnConstraintsPointerFact>();
        } else {
          input_facts[inst] = prev;
        }
        auto out = std::make_shared<ReturnConstraintsPointerFact>();
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

void ReturnConstraintsPointer::runOnFunction(Function &F) {
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
          ii->print(llvm::errs());
          cerr << "\n";
          output_facts.at(&*ii)->dump();
        }
      }
    }
  }
  return;
}

bool ReturnConstraintsPointer::visitBlock(BasicBlock &BB) {
  bool changed = false;
  for (auto ii = BB.begin(), ie = BB.end(); ii != ie; ++ii) {
    Instruction &I = *ii;

    shared_ptr<ReturnConstraintsPointerFact> input_fact = input_facts.at(&I);
    shared_ptr<ReturnConstraintsPointerFact> output_fact = output_facts.at(&I);

    ReturnConstraintsPointerFact prev_fact = *output_fact;
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

void ReturnConstraintsPointer::visitCallInst(
    CallInst &I, shared_ptr<const ReturnConstraintsPointerFact> in,
    shared_ptr<ReturnConstraintsPointerFact> out) {
  out->value = in->value;
  unordered_set<Value *> gen_value({&I});

  string callee_name = getCalleeName(I);

  Constraint c(callee_name);
  c.interval = Interval::TOP;

  out->value[callee_name] = c;
}

void ReturnConstraintsPointer::visitBranchInst(
    BranchInst &I, shared_ptr<const ReturnConstraintsPointerFact> in,
    shared_ptr<ReturnConstraintsPointerFact> out) {
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

  ReturnPropagationPointer *return_propagation = &getAnalysis<ReturnPropagationPointer>();

  BasicBlock *true_bb = dyn_cast<BasicBlock>(I.getOperand(2));
  assert(true_bb);
  BasicBlock *false_bb = dyn_cast<BasicBlock>(I.getOperand(1));
  assert(false_bb);
  Interval true_abstract_value = abstractICmp(*icmp).first;
  Interval false_abstract_value = abstractICmp(*icmp).second;

  // Get the set of function whose values reach icmp operand from
  // return-propagation
  Value *icmp_condition = icmp->getOperand(0);

  auto fact = return_propagation->getInputFactAt(icmp);

  // test_ret_values is set of values representing
  // the return values being tested by this icmp instruction.
  // these may or may not be call instructions (tested in loop below)
  unordered_set<Value*> test_ret_values = fact->getHeldValues(icmp_condition);

  for (Value *v : test_ret_values) {
    ReturnConstraintsPointerFact true_fact;
    ReturnConstraintsPointerFact false_fact;

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
void ReturnConstraintsPointer::visitPHINode(PHINode &I,
                                     shared_ptr<const ReturnConstraintsPointerFact> in,
                                     shared_ptr<ReturnConstraintsPointerFact> out) {

  out->value = in->value;
}

ReturnConstraintsPointerFact ReturnConstraintsPointer::getInFact(Value *v) const {
  return *(input_facts.at(v));
}

ReturnConstraintsPointerFact ReturnConstraintsPointer::getOutFact(Value *v) const {
  return *(output_facts.at(v));
}

void ReturnConstraintsPointer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<ReturnPropagationPointer>();
  AU.setPreservesAll();
}

char ReturnConstraintsPointer::ID = 0;
static RegisterPass<ReturnConstraintsPointer>
    X("return-constraints-pointer",
      "Function Return Value Constraints on each basic block", false, false);
