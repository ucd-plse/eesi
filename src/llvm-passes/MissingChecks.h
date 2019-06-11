#ifndef MISSINGCHECKS_H
#define MISSINGCHECKS_H

#include "ReturnPropagationPointer.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "Constraint.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class MissingChecks : public llvm::ModulePass {
public:
  static char ID;

  MissingChecks() : llvm::ModulePass(ID) {}
  explicit MissingChecks(std::string specs_path, std::string error_only_path, std::string debug_function)
      : llvm::ModulePass(ID), specs_path(specs_path), error_only_path(error_only_path), debug_function(debug_function) {}

  bool runOnModule(llvm::Module &M) override;
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

private:
  std::string specs_path;
  std::string error_only_path;
  std::string debug_function;

  // Function names to check
  std::unordered_map<std::string, Constraint> function_specs;

  // Map from functions to handled functions union of all checks
  std::unordered_map<llvm::Function*, std::unordered_map<std::string, Constraint>> handled_functions;

  // Number of call sites that are checked for each function name
  std::unordered_map<std::string, int> checked_calls;

  // Number of call sites that are not checked for each function name
  std::unordered_map<std::string, int> unchecked_calls;

  // Unchecked call site locations
  std::vector<std::pair<std::string, std::string>> unchecked_locs;

  void readSpecsFile();
  void populateHandledFunctions(llvm::Module &M);

  bool checkIsSufficient(llvm::ICmpInst *icmp, llvm::CallInst *call) const;

  std::unordered_map<llvm::Instruction*, uint64_t> instruction_numbers;
  uint64_t next_inst_num = 0;

  ReturnPropagationPointer *return_propagation = nullptr;
  void visitCallInst(llvm::CallInst *I);

  void readErrorOnlyFile();

  // Error-only functions (from config file)
  std::unordered_set<std::string> error_only;
};

#endif
