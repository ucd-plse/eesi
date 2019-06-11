// Additionally, it computes
// the set of values that a function may return after a call
// to an error-only function. The path to the list of error-only
// functions is an input.

// error_return_values: a map from LLVM functions to the set of integer-like
//                      constants that can be returned after an error-only
//                      function is called. The constants are "integer-like"
//                      because null pointers are represented as 0.

#ifndef ERRORBLOCKS_H
#define ERRORBLOCKS_H

#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Constraint.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

typedef std::pair<std::string, std::string> ErrorPropagationEdge;

struct ErrorBlocks : public llvm::ModulePass {
  static char ID;
  ErrorBlocks() : ModulePass(ID) {}
  ErrorBlocks(std::string error_only_path);
  ErrorBlocks(std::string error_only_path, std::string input_specs_path);

  // Entry point
  bool runOnModule(llvm::Module &M);

  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

  std::unordered_map<std::string, Constraint> getErrorReturnValues() const;

  bool haveAERV(std::string fname) const;
  Constraint getAERV(std::string fname) const;
  bool setAERV(std::string fname, Constraint c);

  // Set of error propagation edges
  std::set<ErrorPropagationEdge> error_propagation;

  // Set of functions with error-only return seeds
  std::unordered_set<std::string> error_only_bootstrap;

private:
  void readErrorOnlyFile(std::string error_only_path);
  void readInputSpecsFile(std::string input_specs_path);

  bool runOnFunction(llvm::Function &F);

  // Called for each basic block
  bool visitBlock(llvm::BasicBlock &BB);
  bool visitCallInst(llvm::CallInst &I);

  // Helper function for adding values to error_return map
  bool addErrorValue(llvm::Function *, int64_t);

  void addErrorPropagation(std::string from, std::string to);

  Interval abstractInteger(int64_t) const;

  // A map from functions to the integer-like constants that
  // may be returned on error
  // Note: the keys for this map will not be identical to the
  // keys in abstract_error_return_values because input error
  // specifications are added to abstract_error_return_values
  // but not error_return_values
  std::unordered_map<llvm::Function *, std::unordered_set<int64_t>>
      error_return_values;

  // ** These are the the function error specifications **
  // A map from function names to the integer-like constants that
  // may be returned on error
  std::unordered_map<std::string, Constraint> abstract_error_return_values;

  std::unordered_map<std::string, Constraint> abstract_success_return_values;

  // Error-only functions (from config file)
  std::unordered_set<std::string> error_only;


  std::unordered_set<int64_t> error_codes = {
    -114, -214, -314, -414, -514, -614, -714, -814, -914, -1014,
    -1114, -1214, -1314, -1414, -1514, -1614, -1714, -1814, -1914,
    -2014, -2114, -2214, -2314, -2414, -2514, -2614, -2714, -2814,
    -2914, -3014, -3114, -3214, -3314, -3414, 114
  };
};

#endif
