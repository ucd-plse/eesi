#ifndef RETURNCONSTRAINTSPOINTER_H
#define RETURNCONSTRAINTSPOINTER_H

#include "Constraint.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_map>
#include <unordered_set>

class ReturnConstraintsPointerFact {
public:
  std::unordered_map<std::string, Constraint> value;

  ReturnConstraintsPointerFact() {}

  // copy constructor
  ReturnConstraintsPointerFact(const ReturnConstraintsPointerFact &other) {
    value = other.value;
  }

  void dump() {
    for (auto kv : value) {
      std::string fn = kv.first;
      std::cerr << fn << ": " << kv.second << " ";
    }
    std::cerr << std::endl;
  }

  bool operator==(const ReturnConstraintsPointerFact &other) {
    return value == other.value;
  }
  bool operator!=(const ReturnConstraintsPointerFact &other) {
    return value != other.value;
  }

  void join(const ReturnConstraintsPointerFact &other) {
    // For each function key, join the constraints
    for (auto kv : other.value) {
      std::string fn = kv.first;
      if (value.find(fn) == value.end()) {
        value[fn] = kv.second;
      }
      value[fn] = value[fn].join(kv.second);
    }
  }

  void meet(const ReturnConstraintsPointerFact &other) {
    // For each function key, meet the constraints
    for (auto kv : other.value) {
      std::string fn = kv.first;
      if (value.find(fn) == value.end()) {
        value[fn] = kv.second;
      }
      value[fn] = value[fn].meet(kv.second);
    }
  }
};

class ReturnConstraintsPointer : public llvm::ModulePass {
public:
  static char ID;

  ReturnConstraintsPointer() : llvm::ModulePass(ID) {}

  // Entry point
  bool runOnModule(llvm::Module &M);

  // Called for each function
  void runOnFunction(llvm::Function &F);

  ReturnConstraintsPointerFact getInFact(llvm::Value *) const;
  ReturnConstraintsPointerFact getOutFact(llvm::Value *) const;

private:
  // Called for each basic block
  bool visitBlock(llvm::BasicBlock &BB);

  // Transfer functions
  void visitReturnInst(llvm::ReturnInst &I,
                       std::shared_ptr<const ReturnConstraintsPointerFact> input,
                       std::shared_ptr<ReturnConstraintsPointerFact> out);
  void visitCallInst(llvm::CallInst &I,
                     std::shared_ptr<const ReturnConstraintsPointerFact> input,
                     std::shared_ptr<ReturnConstraintsPointerFact> out);
  void visitBranchInst(llvm::BranchInst &I,
                       std::shared_ptr<const ReturnConstraintsPointerFact> input,
                       std::shared_ptr<ReturnConstraintsPointerFact> out);
  void visitPHINode(llvm::PHINode &I,
                    std::shared_ptr<const ReturnConstraintsPointerFact> input,
                    std::shared_ptr<ReturnConstraintsPointerFact> out);

  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

  // A map from values (instructions) to dataflow facts
  std::unordered_map<llvm::Value *, std::shared_ptr<ReturnConstraintsPointerFact>>
      input_facts;

  // A map from values (instructions) to dataflow facts
  std::unordered_map<llvm::Value *, std::shared_ptr<ReturnConstraintsPointerFact>>
      output_facts;

};

#endif