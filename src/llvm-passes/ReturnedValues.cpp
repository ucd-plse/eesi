#include <iostream>
#include <string>

#include "Common.h"
#include "ReturnedValues.h"
#include "llvm/IR/CFG.h"

using namespace llvm;
using namespace std;
using namespace errspec;

#define DEBUG false

bool ReturnedValues::runOnModule(Module &M) {
  // Initialize program points to empty ReturnedValuesFact
  // Creates a new fact at every point
  std::shared_ptr<ReturnedValuesFact> prev = nullptr;
  for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    for (auto bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      for (auto ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
        Instruction *inst = &(*ii);
        if (prev == nullptr) {
          input_facts[inst] = std::make_shared<ReturnedValuesFact>();
        } else {
          input_facts[inst] = prev;
        }
        auto out = std::make_shared<ReturnedValuesFact>();
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

void ReturnedValues::runOnFunction(Function &F) {
  string fname = F.getName().str();

  bool changed = true;
  while (changed) {
    changed = false;
    for (auto bi = F.begin(), be = F.end(); bi != be; ++bi) {
      BasicBlock *BB = &*bi;

      Instruction *bb_last = GetLastInstructionOfBB(BB);
      auto bb_out_fact = output_facts.at(bb_last);

      // Go over successor blocks and apply join
      for (auto si = succ_begin(BB), se = succ_end(BB); si != se; ++si) {
        Instruction *succ_first = &(*(si->begin()));
        auto succ_fact = input_facts.at(succ_first);
        bb_out_fact->join(*succ_fact);
      }

      changed = visitBlock(*BB) || changed;
    }
  }

  if (DEBUG) {
    for (auto bi = F.begin(), be = F.end(); bi != be; ++bi) {
      for (auto ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
        cerr << "=====\n";
        input_facts.at(&*ii)->dump();
        cerr << "---\n";
        ii->print(llvm::errs());
        cerr << "\n---\n";
        output_facts.at(&*ii)->dump();
        cerr << "=====\n\n";
      }
    }
  }

  return;
}

bool ReturnedValues::visitBlock(BasicBlock &BB) {
  bool changed = false;
  for (auto ii = BB.rbegin(), ie = BB.rend(); ii != ie; ++ii) {
    Instruction &I = *ii;

    shared_ptr<ReturnedValuesFact> input_fact = input_facts.at(&I);
    shared_ptr<ReturnedValuesFact> output_fact = output_facts.at(&I);

    ReturnedValuesFact prev_fact = *input_fact;
    if (ReturnInst *inst = dyn_cast<ReturnInst>(&I)) {
      visitReturnInst(*inst, input_fact, output_fact);
    } else if (CallInst *inst = dyn_cast<CallInst>(&I)) {
      visitCallInst(*inst, input_fact, output_fact);
    } else if (LoadInst *inst = dyn_cast<LoadInst>(&I)) {
      visitLoadInst(*inst, input_fact, output_fact);
    } else if (StoreInst *inst = dyn_cast<StoreInst>(&I)) {
      visitStoreInst(*inst, input_fact, output_fact);
    } else if (BitCastInst *inst = dyn_cast<BitCastInst>(&I)) {
      visitBitCastInst(*inst, input_fact, output_fact);
    } else if (PtrToIntInst *inst = dyn_cast<PtrToIntInst>(&I)) {
      visitPtrToIntInst(*inst, input_fact, output_fact);
    } else if (TruncInst *inst = dyn_cast<TruncInst>(&I)) {
      visitTruncInst(*inst, input_fact, output_fact);
    } else if (SExtInst *inst = dyn_cast<SExtInst>(&I)) {
      visitSExtInst(*inst, input_fact, output_fact);
    } else if (PHINode *inst = dyn_cast<PHINode>(&I)) {
      visitPHINode(*inst, input_fact, output_fact);
    } else {
      // Default is to just copy facts from previous instruction unchanged.
      input_fact->value = output_fact->value;
    }
    changed = changed || (*(input_fact) != prev_fact);
  }

  return changed;
}

void ReturnedValues::addReturnPropagated(Function *f, string v) {
  if (return_propagated.find(f) == return_propagated.end()) {
    return_propagated[f] = unordered_set<string>({v});
  } else {
    return_propagated[f].insert(v);
  }
}

void ReturnedValues::visitCallInst(CallInst &I,
                                   shared_ptr<ReturnedValuesFact> in,
                                   shared_ptr<const ReturnedValuesFact> out) {
  in->value = out->value;

  string fname = getCalleeName(I);
  if (fname.empty())
    return;
  Function *parent = I.getParent()->getParent();

  // Add every call instruction that can be returned to return propagated map
  if (out->value.find(&I) != out->value.end()) {
    addReturnPropagated(parent, fname);
  }

  // Model ERR_PTR
  // LLVM creates multiple copies with numbers at end ERR_PTR116
  if (fname.find("ERR_PTR") != string::npos) {
    Value *err = I.getOperand(0);
    if (out->value.find(&I) != out->value.end()) {
      in->value.insert(err);
    }
  }

  // Model IS_ERR
  if (fname.find("IS_ERR") != string::npos) {
    Value *err = I.getOperand(0);
    if (out->value.find(&I) != out->value.end()) {
      in->value.insert(err);
    }
  }

  // Model PTR_ERR
  if (fname.find("PTR_ERR") != string::npos) {
    Value *err = I.getOperand(0);
    if (out->value.find(&I) != out->value.end()) {
      in->value.insert(err);
    }
  }

  // Model ERR_CAST
  if (fname.find("PTR_ERR") != string::npos) {
    Value *err = I.getOperand(0);
    if (out->value.find(&I) != out->value.end()) {
      in->value.insert(err);
    }
  }
}

// Insert the value being returned
void ReturnedValues::visitReturnInst(ReturnInst &I,
                                     shared_ptr<ReturnedValuesFact> in,
                                     shared_ptr<const ReturnedValuesFact> out) {

  // check for void return
  if (I.getNumOperands() == 0)
    return;
  Value *returned = I.getOperand(0);
  in->value = out->value;
  in->value.insert(returned);
}

// Add sender if receiver element of out fact, remove receiver from in fact
void ReturnedValues::visitStoreInst(StoreInst &I,
                                    shared_ptr<ReturnedValuesFact> in,
                                    shared_ptr<const ReturnedValuesFact> out) {
  in->value = out->value;
  Value *sender = I.getOperand(0);
  Value *receiver = I.getOperand(1);
  in->value.erase(receiver);
  if (out->value.find(receiver) != out->value.end()) {
    in->value.insert(sender);
  }
}

// Add operand to in fact if load element of out fact
void ReturnedValues::visitLoadInst(LoadInst &I,
                                   shared_ptr<ReturnedValuesFact> in,
                                   shared_ptr<const ReturnedValuesFact> out) {
  in->value = out->value;
  Value *load_from = I.getOperand(0);
  in->value.erase(&I);
  if (out->value.find(&I) != out->value.end()) {
    in->value.insert(load_from);
  }
}

// Same as load
void ReturnedValues::visitBitCastInst(
    BitCastInst &I, shared_ptr<ReturnedValuesFact> in,
    std::shared_ptr<const ReturnedValuesFact> out) {
  in->value = out->value;
  Value *load_from = I.getOperand(0);
  in->value.erase(&I);
  if (out->value.find(&I) != out->value.end()) {
    in->value.insert(load_from);
  }
}

// Same as load
void ReturnedValues::visitPtrToIntInst(
    PtrToIntInst &I, shared_ptr<ReturnedValuesFact> in,
    std::shared_ptr<const ReturnedValuesFact> out) {
  in->value = out->value;
  Value *load_from = I.getOperand(0);
  in->value.erase(&I);
  if (out->value.find(&I) != out->value.end()) {
    in->value.insert(load_from);
  }
}

// Same as load
void ReturnedValues::visitTruncInst(
    TruncInst &I, shared_ptr<ReturnedValuesFact> in,
    std::shared_ptr<const ReturnedValuesFact> out) {
  in->value = out->value;
  in->value.erase(&I);
  Value *load_from = I.getOperand(0);
  if (out->value.find(&I) != out->value.end()) {
    in->value.insert(load_from);
  }
}

// Same as load
void ReturnedValues::visitSExtInst(
    SExtInst &I, shared_ptr<ReturnedValuesFact> in,
    std::shared_ptr<const ReturnedValuesFact> out) {
  in->value = out->value;
  in->value.erase(&I);
  Value *load_from = I.getOperand(0);
  if (out->value.find(&I) != out->value.end()) {
    in->value.insert(load_from);
  }
}

// If the PHI result can be returned, then add incoming values
// to the exit of each incoming basic block
void ReturnedValues::visitPHINode(PHINode &I, shared_ptr<ReturnedValuesFact> in,
                                  shared_ptr<const ReturnedValuesFact> out) {

  in->value = out->value;
  if (out->value.find(&I) == out->value.end()) {
    return;
  }
  in->value.erase(&I);

  for (unsigned i = 0, e = I.getNumIncomingValues(); i != e; ++i) {
    Value *v = I.getIncomingValue(i);
    BasicBlock *BB = I.getIncomingBlock(i);
    Instruction *bb_last = GetLastInstructionOfBB(BB);

    // insert v into the output fact of the last instruction
    auto bb_out_fact = output_facts.at(bb_last);
    bb_out_fact->value.insert(v);
  }
}

ReturnedValuesFact ReturnedValues::getInFact(Value *v) const {
  return *(input_facts.at(v));
}

ReturnedValuesFact ReturnedValues::getOutFact(Value *v) const {
  return *(output_facts.at(v));
}

void ReturnedValues::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

std::unordered_map<llvm::Function *, std::unordered_set<string>>
ReturnedValues::getReturnPropagation() const {
  return return_propagated;
}
char ReturnedValues::ID = 0;
static RegisterPass<ReturnedValues>
    X("returned-values", "Functions that might be returned at each basic block",
      false, false);
