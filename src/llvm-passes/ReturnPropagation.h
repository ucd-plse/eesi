#ifndef RETURNPROPAGATION_H
#define RETURNPROPAGATION_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_map>
#include <unordered_set>

// A dataflow fact is a map from LLVM values to the functions they hold return
// values for
class ReturnPropagationFact {
public:
  std::unordered_map<llvm::Value *, std::unordered_set<llvm::Value *>> value;

  ReturnPropagationFact() {}

  // copy constructor
  ReturnPropagationFact(const ReturnPropagationFact &other) {
    value = other.value;
  }

  void dump() {
    for (auto i : value) {
      llvm::errs() << &(*i.first) << " ";
      i.first->print(llvm::errs());
      llvm::errs() << " <-- ";
      for (auto f : i.second) {
        llvm::errs() << &f << " ";
        f->print(llvm::errs());
      }
      llvm::errs() << "\n";
    }
  }

  bool operator==(const ReturnPropagationFact &other) {
    return value == other.value;
  }
  bool operator!=(const ReturnPropagationFact &other) {
    return value != other.value;
  }

  void join(const ReturnPropagationFact &other) {
    // Union each set in map
    for (auto kv : other.value) {
      value[kv.first].insert(kv.second.begin(), kv.second.end());
    }
  }
};

struct ReturnPropagation : public llvm::ModulePass {
  static char ID;

  ReturnPropagation() : llvm::ModulePass(ID) {}

  // Dataflow facts at the program point immediately following instruction
  std::unordered_map<llvm::Value *, std::shared_ptr<ReturnPropagationFact>>
      input_facts;
  std::unordered_map<llvm::Value *, std::shared_ptr<ReturnPropagationFact>>
      output_facts;

  bool finished = false;

  bool runOnModule(llvm::Module &M);
  bool runOnFunction(llvm::Function &F);
  bool visitBlock(llvm::BasicBlock &BB);

  void visitCallInst(llvm::CallInst &I,
                     std::shared_ptr<const ReturnPropagationFact> input,
                     std::shared_ptr<ReturnPropagationFact> out);
  void visitLoadInst(llvm::LoadInst &I,
                     std::shared_ptr<const ReturnPropagationFact> input,
                     std::shared_ptr<ReturnPropagationFact> out);
  void visitStoreInst(llvm::StoreInst &I,
                      std::shared_ptr<const ReturnPropagationFact> input,
                      std::shared_ptr<ReturnPropagationFact> out);
  void visitBitCastInst(llvm::BitCastInst &I,
                        std::shared_ptr<const ReturnPropagationFact> input,
                        std::shared_ptr<ReturnPropagationFact> out);
  void visitPtrToIntInst(llvm::PtrToIntInst &I,
                         std::shared_ptr<const ReturnPropagationFact> input,
                         std::shared_ptr<ReturnPropagationFact> out);
  void visitBinaryOperator(llvm::BinaryOperator &I,
                         std::shared_ptr<const ReturnPropagationFact> input,
                         std::shared_ptr<ReturnPropagationFact> out);
  void visitPHINode(llvm::PHINode &I,
                    std::shared_ptr<const ReturnPropagationFact> input,
                    std::shared_ptr<ReturnPropagationFact> out);

  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
};

#endif