#ifndef CALLEDFUNCTIONS_H
#define CALLEDFUNCTIONS_H

#include <string>
#include <unordered_set>

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"

struct CalledFunctions : public llvm::FunctionPass {
  static char ID;
  CalledFunctions() : FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override;
  std::unordered_set<std::string> getCalledFunctions();

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

 private:
  std::unordered_set<std::string> called_functions;
};

#endif
