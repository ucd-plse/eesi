#include "MissingChecks.h"
#include "Common.h"
#include "ReturnPropagationPointer.h"
#include "ReturnConstraintsPointer.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <string>
#include <fstream>

using namespace llvm;
using namespace std;
using namespace errspec;

// If set to true, then bugs in void returning functions are filtered out
#define VOIDFILTER false

// The return value must be checked or propagated, and there must exist an error
// handling block for the function in the same parent. False negatives if >1
// call to same function, both checked, and one has an insufficient check.

bool MissingChecks::runOnModule(llvm::Module &M) {
  readSpecsFile();
  readErrorOnlyFile();
  populateHandledFunctions(M);

  return_propagation = &getAnalysis<ReturnPropagationPointer>();

  for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    Function *f = &*fi;
    for (auto bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      for (auto ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
        instruction_numbers[&(*ii)] = next_inst_num++;
      }
    }
  }

  for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    Function *f = &*fi;
    for (auto bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      for (auto ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
        if (CallInst *call = dyn_cast<CallInst>(&(*ii))) {
          visitCallInst(call);
        }
      }
    }
  }

  for (auto ul : unchecked_locs) {
    string fname = ul.first;
    string loc = ul.second;
    cout << loc << " " << fname << " " << unchecked_calls.at(fname) << " "
         << checked_calls.at(fname) << endl;
  }
}

void MissingChecks::populateHandledFunctions(llvm::Module &M) {
  // Generate map from functions to handled functions
  ReturnConstraintsPointer &return_constraints = getAnalysis<ReturnConstraintsPointer>();

  for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    Function *f = &(*fi);
    if (handled_functions.find(f) == handled_functions.end()) {
      handled_functions[f] = unordered_map<string, Constraint>();
    }
    for (auto bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
      Instruction *bb_last = GetLastInstructionOfBB(&(*bi));
      ReturnConstraintsPointerFact rcf = return_constraints.getOutFact(bb_last);

      for (auto kv : rcf.value) {
        string constraint_fname = kv.first;
        Constraint block_constraint = kv.second;

        if (function_specs.find(constraint_fname) == function_specs.end()) {
          continue;
        }
        if (block_constraint.interval == Interval::TOP) {
          continue;
        }

        Constraint spec = function_specs.at(block_constraint.fname);
        if (block_constraint.fname != spec.fname) {
          continue;
        }

        if (handled_functions[f].find(constraint_fname) == handled_functions[f].end()) {
          Constraint empty = Constraint(constraint_fname);
          empty.interval = Interval::BOT;
          handled_functions[f][constraint_fname] = empty;
        }
        if (block_constraint.covers(spec.interval) || spec.covers(block_constraint.interval)) {
          handled_functions[f][constraint_fname] = handled_functions[f][constraint_fname].join(block_constraint);
        }
      }
    }
  }
}

void MissingChecks::readSpecsFile() {
  string line;

  // Read input specifications from text file line by line
  ifstream specs_file(specs_path);
  bool have_specs = false;
  while (getline(specs_file, line)) {
    have_specs = true;
    // need to split fname from spec
    vector<string> fields;
    boost::split(fields, line, boost::is_any_of(" "));
    string fname = fields[1];
    Constraint c(fname, fields[2]);
    function_specs[fname] = c;
  }

  if (!have_specs) {
    cerr << "WARNING: EMPTY INPUT SPECS LIST!\n";
  }
}

void MissingChecks::readErrorOnlyFile() {
  string line;

  // Read error-only functions from text file line by line
  // and insert them into the set of error-only functions.
  ifstream error_only_file(error_only_path);
  while (getline(error_only_file, line)) {
    error_only.insert(line);
  }
  if (error_only.size() == 0) {
    cerr << "WARNING: EMPTY ERROR-ONLY SET!\n";
  }
}


void MissingChecks::visitCallInst(llvm::CallInst *I) {
  ReturnConstraintsPointer &return_constraints = getAnalysis<ReturnConstraintsPointer>();

  Function *f = dyn_cast<Function>(I->getCalledValue()->stripPointerCasts());
  if (!f)
    return;

  string fname = getCalleeName(*I);
  if (error_only.find(fname) != error_only.end()) {
    BasicBlock *bb = I->getParent();
    Instruction *bb_last = GetLastInstructionOfBB(bb);
    ReturnConstraintsPointerFact rcf = return_constraints.getOutFact(bb_last);

    bool haveSuccess = false;
    bool haveNoError = true;
    Constraint success_constraint;
    Constraint error_spec;

    for (auto kv : rcf.value) {
      string constraint_fname = kv.first;
      Constraint block_constraint = kv.second;

      if (function_specs.find(constraint_fname) == function_specs.end()) {
        continue;
      }
      if (block_constraint.interval == Interval::TOP) {
        continue;
      }

      Constraint spec = function_specs.at(block_constraint.fname);
      if (block_constraint.fname != spec.fname) {
        continue;
      }

      if (block_constraint.meet(spec).interval == Interval::BOT) {
        haveSuccess = true;
        success_constraint = block_constraint;
        error_spec = spec;
      }
      if (block_constraint.meet(spec).interval != Interval::BOT) {
        haveNoError = false;
      }
    }

    if (haveSuccess && haveNoError) {
      string file;
      unsigned line;
      if (DILocation *loc = I->getDebugLoc()) {
        file = loc->getFilename();
        line = loc->getLine();
      }

      bool short_distance_to_call = false;
      Module &M = *(I->getParent()->getParent()->getParent());
      for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
        Function *f = &*fi;
        for (auto bi = fi->begin(), be = fi->end(); bi != be; ++bi) {
          for (auto ii = bi->begin(), ie = bi->end(); ii != ie; ++ii) {
            if (CallInst *call = dyn_cast<CallInst>(&(*ii))) {
              if (getCalleeName(*call) == success_constraint.fname) {
                uint64_t call_instruction_number = instruction_numbers.at(call);
                uint64_t eo_instruction_number = instruction_numbers.at(I);
                if (eo_instruction_number - call_instruction_number <= 25) {
                  short_distance_to_call = true;
                }
              }
            }
          }
        }
      }


      if (short_distance_to_call) {
        string loc = file + ":" + to_string(line);
        cout << loc << " " << 
          success_constraint.fname << " " << success_constraint.interval << " "
          << error_spec.fname << " " << error_spec.interval << endl;
      }
    }
  }

  if (function_specs.find(fname) == function_specs.end()) {
    return;
  }

  Function *p = I->getParent()->getParent();

  // Set to true if we find a check (increment checked calls)
  bool checked = false;

  // Set to true if we know we can't reason about this call (do not increment)
  bool filtered = false;

  // Iterate over every instruction in the parent function
  for (inst_iterator ii = inst_begin(*p), ie = inst_end(*p); ii != ie; ++ii) {
    Instruction *inst = &*ii;
    shared_ptr<ReturnPropagationPointerFact> input_fact = 
      return_propagation->getInputFactAt(inst);
    
    if (ICmpInst *icmp = dyn_cast<ICmpInst>(inst)) {
      if (checkIsSufficient(icmp, I)) {
        checked = true;
      }

    } else if (ReturnInst *ret = dyn_cast<ReturnInst>(inst)) {
      if (inst->getNumOperands() > 0) {
        Value *returned = inst->getOperand(0);
        if (input_fact->valueMayHold(returned, I)) {
          checked = true;
        }
      }
    } else if (CallInst *call = dyn_cast<CallInst>(inst)) {
      string fname = getCalleeName(*call);
      if (fname.find("IS_ERR") != string::npos) {
        for (int i = 0; i < call->getNumArgOperands(); ++i) {
          Value *op = call->getArgOperand(i);
          if (input_fact->valueMayHold(op, I)) {
            checked = true;
          }
        }
      }
    } else if (SwitchInst *swtch = dyn_cast<SwitchInst>(inst)) {
      Value *op = swtch->getCondition();
      if (input_fact->valueMayHold(op, I)) {
        checked = true;
      }
    }
  }

  if (VOIDFILTER && I->getParent()->getParent()->getReturnType()->isVoidTy()) {
    filtered = true;
  }

  if (checked_calls.find(fname) == checked_calls.end()) {
    checked_calls[fname] = 0;
  }
  if (unchecked_calls.find(fname) == unchecked_calls.end()) {
    unchecked_calls[fname] = 0;
  }

  if (!checked && !filtered) {
    // Get the source location of the call and print that out
    string file;
    unsigned line;
    if (DILocation *loc = I->getDebugLoc()) {
      file = loc->getFilename();
      line = loc->getLine();
    }

    unchecked_calls[fname] = unchecked_calls[fname] + 1;
    string loc = file + ":" + to_string(line);
    unchecked_locs.push_back(make_pair(fname, loc));
  } else {
    checked_calls[fname] = checked_calls[fname] + 1;
  }
}

bool MissingChecks::checkIsSufficient(ICmpInst *icmp, CallInst *call) const {
  shared_ptr<ReturnPropagationPointerFact> input_fact = 
    return_propagation->getInputFactAt(icmp);

  string fname = getCalleeName(*call);
  Function *parent = icmp->getParent()->getParent();

  if (function_specs.find(fname) == function_specs.end()) {
    return false;
  }
  Constraint spec = function_specs.at(fname);

  // Check the first icmp operand
  Value *op0 = icmp->getOperand(0);
  if (input_fact->valueMayHold(op0, call)) {
    auto checked_unions = handled_functions.at(parent);
    if (checked_unions.find(fname) == checked_unions.end()) {
      return false;
    }
    Constraint checked_union = checked_unions.at(fname);
    if (checked_union.covers(spec.interval)) {
      return true;
    }
  }

  // Check the second icmp operand
  Value *op1 = icmp->getOperand(1);
  if (input_fact->valueMayHold(op1, call)) {
    auto checked_unions = handled_functions.at(parent);
    if (checked_unions.find(fname) == checked_unions.end()) {
      return false;
    }
    Constraint checked_union = checked_unions.at(fname);
    if (checked_union.covers(spec.interval)) {
      return true;
    }
  }

  return false;
}

void MissingChecks::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<ReturnPropagationPointer>();
  AU.addRequired<ReturnConstraintsPointer>();
  AU.setPreservesAll();
}

char MissingChecks::ID = 0;
static RegisterPass<MissingChecks>
    X("missing-checks",
      "Find unchecked calls to functions with error specifications", false,
      false);
