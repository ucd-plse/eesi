#ifndef RETURNPROPAGATIONPOINTER_H
#define RETURNPROPAGATIONPOINTER_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <iostream>
#include <boost/functional/hash.hpp>

// MemVals can be either LLVM values or MemIndexes
class MemVal {
 public:
  explicit MemVal(llvm::Value *v) : base_value(v) {
    is_ref = false;
  }
  explicit MemVal(uint64_t idx) : base_idx(idx) {
    is_ref = true;
  }
  MemVal() {}

  bool isRef() const { return is_ref; }

  void dump() const {
    std::cerr << "(";
    if (isRef()) {
      std::cerr << base_idx;
    } else {
      base_value->print(llvm::errs());
    }
    std::cerr << "-" << idx1 << "-" << idx2 << ")";
  }

  size_t hash() const {
    std::size_t res = 0;
    boost::hash_combine(res, base_value);
    boost::hash_combine(res, base_idx);
    boost::hash_combine(res, idx1);
    boost::hash_combine(res, idx2);
    return res;
  }

  bool operator==(const MemVal &other) const {
    return base_value == other.base_value
      && base_idx == other.base_idx
      && idx1 == other.idx1
      && idx2 == other.idx2;
  }
  
  bool operator!=(const MemVal &other) const{
    return !(*this == other);
  }

  // Only one of these can be set at a time
  bool is_ref;
  llvm::Value* base_value = nullptr;
  uint64_t base_idx = 0;
  uint64_t idx1 = 0;
  uint64_t idx2 = 0;
};

namespace std {
  template <>
  struct hash<MemVal>
  {
      size_t operator()(const MemVal& m) const
      {
        return m.hash();
      }
  };
}

class ReturnPropagationPointerFact {
  friend class ReturnPropagationPointer;

 public:
  ReturnPropagationPointerFact() {}

  // copy constructor
  ReturnPropagationPointerFact(const ReturnPropagationPointerFact &other) {
    value = other.value;
  }

  void dump() const {
    for (auto mv : value) {
      MemVal idx = mv.first;
      idx.dump();
      std::cerr << ": ";

      std::unordered_set<MemVal> mem_vals = mv.second;
      for (auto v : mem_vals) {
        v.dump();
      }
      std::cerr << "\n";
    }
  }

  bool operator==(const ReturnPropagationPointerFact &other) {
    return value == other.value;
  }
  bool operator!=(const ReturnPropagationPointerFact &other) {
    return value != other.value;
  }

  void join(const ReturnPropagationPointerFact &other) {
    // Union each set in map
    for (auto kv : other.value) {
      value[kv.first].insert(kv.second.begin(), kv.second.end());
    }
  }

  bool valueMayHold(llvm::Value *var, llvm::CallInst *) const;

  // Get the LLVM values that this fact may hold
  std::unordered_set<llvm::Value*> getHeldValues(llvm::Value *var) const;

 private:
    // The memory model
    std::unordered_map<MemVal, std::unordered_set<MemVal>> value;
};

class ReturnPropagationPointer : public llvm::ModulePass {
public:    
  static char ID;

  ReturnPropagationPointer() : llvm::ModulePass(ID) {}
  ReturnPropagationPointer(std::string debug_function) : llvm::ModulePass(ID), debug_function(debug_function) {}

  bool runOnModule(llvm::Module &M);
  bool runOnFunction(llvm::Function &F);
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

  std::string debug_function;
  std::shared_ptr<ReturnPropagationPointerFact> getInputFactAt(llvm::Value *v) const;
  std::shared_ptr<ReturnPropagationPointerFact> getOutputFactAt(llvm::Value *v) const;

private:
  // Dataflow facts at the program point immediately following instruction
  std::unordered_map<llvm::Value *, std::shared_ptr<ReturnPropagationPointerFact>>
      input_facts;
  std::unordered_map<llvm::Value *, std::shared_ptr<ReturnPropagationPointerFact>>
      output_facts;

  uint64_t next_idx = 0;

  bool finished = false;

  // If memory is going to be written to, use this to prevent creation of
  // extra reference MemVals
  std::unordered_set<MemVal>& findOrCreateMemVal(ReturnPropagationPointerFact &rpf, const MemVal &idx);
  std::unordered_set<MemVal>& findOrCreateMemValEmpty(ReturnPropagationPointerFact &rpf, const MemVal &idx);
  std::unordered_set<MemVal>& createMemValEmpty(ReturnPropagationPointerFact &rpf, const MemVal &idx);
  bool haveMemVal(const ReturnPropagationPointerFact &rpf, const MemVal &idx);

  bool visitBlock(llvm::BasicBlock &BB);

  void visitCallInst(llvm::CallInst &I,
                     std::shared_ptr<const ReturnPropagationPointerFact> input,
                     std::shared_ptr<ReturnPropagationPointerFact> out);
  void visitLoadInst(llvm::LoadInst &I,
                     std::shared_ptr<const ReturnPropagationPointerFact> input,
                     std::shared_ptr<ReturnPropagationPointerFact> out);
  void visitStoreInst(llvm::StoreInst &I,
                      std::shared_ptr<const ReturnPropagationPointerFact> input,
                      std::shared_ptr<ReturnPropagationPointerFact> out);
  void visitBitCastInst(llvm::BitCastInst &I,
                        std::shared_ptr<const ReturnPropagationPointerFact> input,
                        std::shared_ptr<ReturnPropagationPointerFact> out);
  void visitPtrToIntInst(llvm::PtrToIntInst &I,
                         std::shared_ptr<const ReturnPropagationPointerFact> input,
                         std::shared_ptr<ReturnPropagationPointerFact> out);
  void visitBinaryOperator(llvm::BinaryOperator &I,
                         std::shared_ptr<const ReturnPropagationPointerFact> input,
                         std::shared_ptr<ReturnPropagationPointerFact> out);
  void visitGetElementPtrInst(llvm::GetElementPtrInst &I,
                         std::shared_ptr<const ReturnPropagationPointerFact> input,
                         std::shared_ptr<ReturnPropagationPointerFact> out);
  void visitAllocaInst(llvm::AllocaInst &I,
                         std::shared_ptr<const ReturnPropagationPointerFact> input,
                         std::shared_ptr<ReturnPropagationPointerFact> out);
  void visitPHINode(llvm::PHINode &I,
                    std::shared_ptr<const ReturnPropagationPointerFact> input,
                    std::shared_ptr<ReturnPropagationPointerFact> out);

  std::unordered_set<MemVal> calculateGEP(ReturnPropagationPointerFact &rpf, llvm::GetElementPtrInst &I);
};

#endif