#include <iostream>
#include <memory>
#include <string>

#include "ReturnPropagation.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AliasSetTracker.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace std;

#define DEBUG false

bool ReturnPropagation::runOnModule(Module &M) {
  if (finished)
    return false;

  std::shared_ptr<ReturnPropagationFact> prev = nullptr;
  for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    for (auto bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      for (auto ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
        // Initialize every instruction to empty
        // Creates a new fact at every point
        Instruction *inst = &(*ii);
        if (prev == nullptr) {
          input_facts[inst] = std::make_shared<ReturnPropagationFact>();
        } else {
          input_facts[inst] = prev;
        }
        auto out = std::make_shared<ReturnPropagationFact>();
        output_facts[inst] = out;
        prev = out;

      }
      prev = nullptr;
    }
  }

  for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    runOnFunction(*fi);
  }

  if (DEBUG) {
    for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
      for (auto bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
        for (auto ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
          cerr << "=====\n";
          input_facts.at(&*ii)->dump();
          cerr << "---\n";
          ii->dump();
          cerr << "---\n";
          output_facts.at(&*ii)->dump();
          cerr << "=====\n\n";
        }
      }
    }
  }

  finished = true;

  return false;
}

bool ReturnPropagation::runOnFunction(Function &F) {
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
  }

  return false;
}

bool ReturnPropagation::visitBlock(BasicBlock &BB) {
  bool changed = false;
  for (auto ii = BB.begin(), ie = BB.end(); ii != ie; ++ii) {
    Instruction &I = *ii;

    shared_ptr<ReturnPropagationFact> input_fact = input_facts.at(&I);
    shared_ptr<ReturnPropagationFact> output_fact = output_facts.at(&I);

    bool changed = false;
    if (CallInst *inst = dyn_cast<CallInst>(&I)) {
      visitCallInst(*inst, input_fact, output_fact);
    } else if (LoadInst *inst = dyn_cast<LoadInst>(&I)) {
      visitLoadInst(*inst, input_fact, output_fact);
    } else if (StoreInst *inst = dyn_cast<StoreInst>(&I)) {
      visitStoreInst(*inst, input_fact, output_fact);
    } else if (BitCastInst *inst = dyn_cast<BitCastInst>(&I)) {
      visitBitCastInst(*inst, input_fact, output_fact);
    } else if (PtrToIntInst *inst = dyn_cast<PtrToIntInst>(&I)) {
      visitPtrToIntInst(*inst, input_fact, output_fact);
    } else if (BinaryOperator *inst = dyn_cast<BinaryOperator>(&I)) {
      visitBinaryOperator(*inst, input_fact, output_fact);
    } else if (PHINode *inst = dyn_cast<PHINode>(&I)) {
      visitPHINode(*inst, input_fact, output_fact);
    } else {
      // Default is to just copy facts from previous instruction unchanged.
      output_fact->value = input_fact->value;
    }

    changed = (*input_fact == *output_fact);
  }

  return changed;
}

void ReturnPropagation::visitCallInst(
    CallInst &I, shared_ptr<const ReturnPropagationFact> in,
    shared_ptr<ReturnPropagationFact> out) {

  out->value = in->value;

  // insert
  if (out->value.find(&I) == out->value.end()) {
    out->value[&I] = unordered_set<Value *>();
  }
  out->value.at(&I).insert(&I);
}

// Copy the return facts into a new value
void ReturnPropagation::visitLoadInst(
    LoadInst &I, shared_ptr<const ReturnPropagationFact> in,
    shared_ptr<ReturnPropagationFact> out) {

  out->value = in->value;
  Value *load_from = I.getOperand(0);

  if (in->value.find(load_from) != in->value.end()) {
    out->value[&I] = in->value.at(load_from);
  }
}

// Copy the return facts into a new value
void ReturnPropagation::visitStoreInst(
    StoreInst &I, shared_ptr<const ReturnPropagationFact> in,
    shared_ptr<ReturnPropagationFact> out) {

  Value *sender = I.getOperand(0);
  Value *receiver = I.getOperand(1);

  out->value = in->value;

  if (ConstantInt *sender_int = dyn_cast<ConstantInt>(sender)) {
    // insert
    if (out->value.find(receiver) == out->value.end()) {
      out->value[receiver] = unordered_set<Value *>();
    }
    out->value.at(receiver).insert(sender);
  }

  if (in->value.find(sender) != in->value.end()) {
    out->value[receiver] = in->value.at(sender);
  }
}

void ReturnPropagation::visitBitCastInst(
    llvm::BitCastInst &I, shared_ptr<const ReturnPropagationFact> in,
    shared_ptr<ReturnPropagationFact> out) {

  // Identical to load
  out->value = in->value;
  Value *load_from = I.getOperand(0);
  if (in->value.find(load_from) != in->value.end()) {
    out->value[&I] = in->value.at(load_from);
  }
}

void ReturnPropagation::visitPtrToIntInst(
    llvm::PtrToIntInst &I, shared_ptr<const ReturnPropagationFact> in,
    shared_ptr<ReturnPropagationFact> out) {

  // Identical to load
  out->value = in->value;
  Value *load_from = I.getOperand(0);
  if (in->value.find(load_from) != in->value.end()) {
    out->value[&I] = in->value.at(load_from);
  }
}

void ReturnPropagation::visitBinaryOperator(
    llvm::BinaryOperator &I, shared_ptr<const ReturnPropagationFact> in,
    shared_ptr<ReturnPropagationFact> out) {

  // Identical to load
  out->value = in->value;
  Value *load_from = I.getOperand(0);
  if (in->value.find(load_from) != in->value.end()) {
    out->value[&I] = in->value.at(load_from);
  }
}

void ReturnPropagation::visitPHINode(llvm::PHINode &I,
                                     shared_ptr<const ReturnPropagationFact> in,
                                     shared_ptr<ReturnPropagationFact> out) {

  // Union all of the sets together for phi incoming values
  for (unsigned i = 0, e = I.getNumIncomingValues(); i != e; ++i) {
    Value *v = I.getIncomingValue(i);
    if (in->value.find(v) != in->value.end()) {
      for (Value *from : in->value.at(v)) {
        out->value[&I].insert(from);
      }
    }
  }
}

void ReturnPropagation::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

char ReturnPropagation::ID = 0;
static RegisterPass<ReturnPropagation>
    X("return-propagation", "Map from LLVM value to return values it may hold",
      false, false);
