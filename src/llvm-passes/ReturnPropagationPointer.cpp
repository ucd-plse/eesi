#include <iostream>
#include <memory>
#include <string>

#include "ReturnPropagationPointer.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AliasSetTracker.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace std;

#define DEBUG false

bool ReturnPropagationPointer::runOnModule(Module &M) {
  if (finished)
    return false;

  std::shared_ptr<ReturnPropagationPointerFact> prev = nullptr;
  for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    for (auto bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      for (auto ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
        // Initialize every instruction to empty
        // Creates a new fact at every point
        Instruction *inst = &(*ii);
        if (prev == nullptr) {
          input_facts[inst] = std::make_shared<ReturnPropagationPointerFact>();
        } else {
          input_facts[inst] = prev;
        }
        auto out = std::make_shared<ReturnPropagationPointerFact>();
        output_facts[inst] = out;
        prev = out;

      }
      prev = nullptr;
    }
  }

  for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    runOnFunction(*fi);
  }

  for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    if (fi->getName() == debug_function) {
      for (auto bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
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
  }

  finished = true;

  return false;
}

bool ReturnPropagationPointer::runOnFunction(Function &F) {
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


// Returns the set of memory addresses that this GEP could point to
// If we have not seen GEP base before this will create a new address
// Otherwise based on the set that GEP base can point to, with indices applied
unordered_set<MemVal> ReturnPropagationPointer::calculateGEP(ReturnPropagationPointerFact &rpf, GetElementPtrInst &I) {
  unordered_set<MemVal> ret;

  if (I.getNumOperands() < 3) {
    ret.insert(MemVal(next_idx++));
    return ret;
  }

  Value *base = I.getOperand(0);
  Value *idx1 = I.getOperand(1);
  Value *idx2 = I.getOperand(2);
  ConstantInt *idx1_int = dyn_cast<ConstantInt>(idx1);
  ConstantInt *idx2_int = dyn_cast<ConstantInt>(idx2);

  // We only support constant indexes
  if (!idx1_int || !idx2_int) {
    ret.insert(MemVal(next_idx++));
    return ret;
  }

  // Lookup the base operand. If we have a memory value for it, then
  // then use that. Otherwise create a new val.
  unordered_set<MemVal> base_mv = findOrCreateMemVal(rpf, MemVal(base));
  for (auto &mv : base_mv) {
    if (mv.isRef()) {
      MemVal gep_base_idx(mv.base_idx);
      gep_base_idx.idx1 = idx1_int->getLimitedValue();
      gep_base_idx.idx2 = idx2_int->getLimitedValue();
      ret.insert(MemVal(gep_base_idx));
    } else {
      MemVal gep_base_idx(mv.base_value);
      gep_base_idx.idx1 = idx1_int->getLimitedValue();
      gep_base_idx.idx2 = idx2_int->getLimitedValue();
      ret.insert(MemVal(gep_base_idx));
    }
  }
  return ret;
}

bool ReturnPropagationPointer::visitBlock(BasicBlock &BB) {
  bool changed = false;
  for (auto ii = BB.begin(), ie = BB.end(); ii != ie; ++ii) {
    Instruction &I = *ii;

    shared_ptr<ReturnPropagationPointerFact> input_fact = input_facts.at(&I);
    shared_ptr<ReturnPropagationPointerFact> output_fact = output_facts.at(&I);

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
    } else if (GetElementPtrInst *inst = dyn_cast<GetElementPtrInst>(&I)) {
      visitGetElementPtrInst(*inst, input_fact, output_fact);
    } else if (AllocaInst *inst = dyn_cast<AllocaInst>(&I)) {
      visitAllocaInst(*inst, input_fact, output_fact);
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

// The value of the instruction is bound to the return value of
// the callee
void ReturnPropagationPointer::visitCallInst(
    CallInst &I, shared_ptr<const ReturnPropagationPointerFact> in,
    shared_ptr<ReturnPropagationPointerFact> out) {

  out->value = in->value;

  if (isa<IntrinsicInst>(&I)) {
    return;
  }

  MemVal idx(&I);
  if (out->value.find(idx) == out->value.end()) {
    out->value[idx] = unordered_set<MemVal>();
  }

  out->value.at(idx).insert(idx);
}

// Copy the return facts into a new value
void ReturnPropagationPointer::visitLoadInst(
    LoadInst &I, shared_ptr<const ReturnPropagationPointerFact> in,
    shared_ptr<ReturnPropagationPointerFact> out) {

  out->value = in->value;
  Value *load_from = I.getOperand(0);

  // MemVals to load from
  unordered_set<MemVal> &load_from_vals = findOrCreateMemVal(*out, MemVal(load_from));

  // Dereference and copy each possible entry
  unordered_set<MemVal> &load_to_val = findOrCreateMemValEmpty(*out, MemVal(&I));

  for (const MemVal &to_deref: load_from_vals) {
    unordered_set<MemVal> &to_deref_vals = findOrCreateMemVal(*out, to_deref);
    for (const MemVal &deref_val : to_deref_vals) {
      load_to_val.insert(deref_val);
    }
  }
}

// When we reference memory for the first time, initialize it with
// a MemVal that points to somewhere disjoint from other memory
unordered_set<MemVal>&
ReturnPropagationPointer::findOrCreateMemVal(ReturnPropagationPointerFact &rpf, const
MemVal &v) {

  if (rpf.value.find(v) == rpf.value.end()) {
    rpf.value[v] = unordered_set<MemVal>();
    rpf.value.at(v).insert(MemVal(next_idx++));
  } 
  return rpf.value.at(v);
}

// When we reference memory for the first time, initialize it with
// a MemVal that points to somewhere disjoint from other memory
unordered_set<MemVal>&
ReturnPropagationPointer::findOrCreateMemValEmpty(ReturnPropagationPointerFact &rpf, const
MemVal &v) {

  if (rpf.value.find(v) == rpf.value.end()) {
    rpf.value[v] = unordered_set<MemVal>();
  } 
  return rpf.value.at(v);
}

// When we reference memory for the first time, initialize it with
// a MemVal that points to somewhere disjoint from other memory
unordered_set<MemVal>&
ReturnPropagationPointer::createMemValEmpty(ReturnPropagationPointerFact &rpf, const
MemVal &v) {
  rpf.value[v] = unordered_set<MemVal>();
  return rpf.value.at(v);
}

bool ReturnPropagationPointer::haveMemVal(const ReturnPropagationPointerFact &rpf, const
MemVal &v) {
  return rpf.value.find(v) != rpf.value.end();
}

// Copy the return facts into a new value
void ReturnPropagationPointer::visitStoreInst(
    StoreInst &I, shared_ptr<const ReturnPropagationPointerFact> in,
    shared_ptr<ReturnPropagationPointerFact> out) {

  out->value = in->value;

  Value *sender = I.getOperand(0);
  Value *receiver = I.getOperand(1);

  unordered_set<MemVal> &receiver_vals = findOrCreateMemVal(*out, MemVal(receiver));

  // v is all of the possible values that the receiver could point to
  for (const auto &v : receiver_vals) {
    unordered_set<MemVal> new_receiver_value;

    // If we have a value for this sender memory address, then 
    // deref the address and store that. Otherwise just store directly.
    MemVal sender_address(sender);
    if (haveMemVal(*in, sender_address)) {
      // The normal case
      new_receiver_value = in->value.at(sender_address);
    } else {
      // Useful for constants, debugging
      new_receiver_value.insert(sender_address);
    }

    // Dereference the receiver val
    out->value[v] = new_receiver_value;
  }
}

// Copy without dereference
void ReturnPropagationPointer::visitBitCastInst(
    llvm::BitCastInst &I, shared_ptr<const ReturnPropagationPointerFact> in,
    shared_ptr<ReturnPropagationPointerFact> out) {

  out->value = in->value;

  // The value being casted
  Value *v = I.getOperand(0);
  MemVal mv(v);

  unordered_set<MemVal> &casted_vals = createMemValEmpty(*out, MemVal(&I));
  casted_vals.insert(mv);
}

void ReturnPropagationPointer::visitPtrToIntInst(
    llvm::PtrToIntInst &I, shared_ptr<const ReturnPropagationPointerFact> in,
    shared_ptr<ReturnPropagationPointerFact> out) {

  out->value = in->value;

  // The value being casted
  Value *v = I.getOperand(0);
  MemVal mv(v);

  unordered_set<MemVal> &casted_vals = createMemValEmpty(*out, MemVal(v));
  casted_vals.insert(mv);
}

void ReturnPropagationPointer::visitBinaryOperator(
    llvm::BinaryOperator &I, shared_ptr<const ReturnPropagationPointerFact> in,
    shared_ptr<ReturnPropagationPointerFact> out) {

  out->value = in->value;

  // The value being casted
  Value *v = I.getOperand(0);
  MemVal mv(v);

  unordered_set<MemVal> &casted_vals = createMemValEmpty(*out, MemVal(v));
  casted_vals.insert(mv);
}

void ReturnPropagationPointer::visitGetElementPtrInst(
    llvm::GetElementPtrInst &I, shared_ptr<const ReturnPropagationPointerFact> in,
    shared_ptr<ReturnPropagationPointerFact> out) {

  out->value = in->value;

  unordered_set<MemVal> &gep_vals = createMemValEmpty(*out, MemVal(&I));
  gep_vals = calculateGEP(*out, I);
}

void ReturnPropagationPointer::visitAllocaInst(
    llvm::AllocaInst &I, shared_ptr<const ReturnPropagationPointerFact> in,
    shared_ptr<ReturnPropagationPointerFact> out) {

  out->value = in->value;
  findOrCreateMemVal(*out, MemVal(&I));
}

void ReturnPropagationPointer::visitPHINode(llvm::PHINode &I,
                                     shared_ptr<const ReturnPropagationPointerFact> in,
                                     shared_ptr<ReturnPropagationPointerFact> out) {
  out->value = in->value;

  MemVal idx(&I);
  if (out->value.find(idx) == out->value.end()) {
    out->value[idx] = unordered_set<MemVal>();
  }

  // Union all of the sets together for phi incoming values
  for (unsigned i = 0, e = I.getNumIncomingValues(); i != e; ++i) {
    Value *v = I.getIncomingValue(i);
    MemVal incoming(v);
    out->value[idx].insert(incoming);
  }
}


shared_ptr<ReturnPropagationPointerFact> ReturnPropagationPointer::getInputFactAt(llvm::Value *v) const {
  return input_facts.at(v);
}

shared_ptr<ReturnPropagationPointerFact> ReturnPropagationPointer::getOutputFactAt(llvm::Value *v) const {
  return output_facts.at(v);
}

bool ReturnPropagationPointerFact::valueMayHold(Value *var, CallInst *call_ret) const {
  bool may_hold = false;

  MemVal mv_var(var);
  if (value.find(mv_var) != value.end()) {
    const unordered_set<MemVal> &op_may_hold = value.at(mv_var);
    MemVal mv_call_ret(call_ret);
    if (op_may_hold.find(mv_call_ret) != op_may_hold.end()) {
      may_hold = true;
    }
  }

  return may_hold;
}

std::unordered_set<Value*> ReturnPropagationPointerFact::getHeldValues(Value *var) const {
  std::unordered_set<Value*> ret;
  MemVal mv_var(var);

  if (value.find(mv_var) == value.end()) return ret;

  for (const MemVal &mv : value.at(mv_var)) {
    if (!mv.isRef()) {
      ret.insert(mv.base_value);
    }
  }

  return ret;
}

void ReturnPropagationPointer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

char ReturnPropagationPointer::ID = 0;
static RegisterPass<ReturnPropagationPointer>
    X("return-propagation-pointer", "Map from LLVM value to return values it may hold",
      false, false);
