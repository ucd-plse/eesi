#ifndef DEFINEDFUNCTIONS_H
#define DEFINEDFUNCTIONS_H

#include <string>
#include <unordered_set>

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"

struct DefinedFunctions : public llvm::FunctionPass {
  static char ID;
  DefinedFunctions() : FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override;
  std::unordered_set<std::string> getDefinedFunctions();

 private:
  std::unordered_set<std::string> defined_functions;
};

#endif
