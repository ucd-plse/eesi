#include "ErrorBlocks.h"
#include "Common.h"
#include "ReturnConstraints.h"
#include "ReturnPropagation.h"
#include "ReturnedValues.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <iostream>
#include <unordered_map>

using namespace llvm;
using namespace std;
using namespace errspec;

bool dbg = false;

ErrorBlocks::ErrorBlocks(string error_only_path) : ModulePass(ID) {
  readErrorOnlyFile(error_only_path);
}

ErrorBlocks::ErrorBlocks(string error_only_path, string input_specs_path)
    : ModulePass(ID) {
  readErrorOnlyFile(error_only_path);
  readInputSpecsFile(input_specs_path);
}

void ErrorBlocks::readErrorOnlyFile(string error_only_path) {
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

void ErrorBlocks::readInputSpecsFile(string input_specs_path) {
  string line;

  // Read input specifications from text file line by line
  // and insert them into AERV map
  ifstream input_specs_file(input_specs_path);
  bool have_input_specs = false;
  while (getline(input_specs_file, line)) {
    have_input_specs = true;
    // need to split fname from spec
    vector<string> fields;
    boost::split(fields, line, boost::is_any_of(" "));
    string fname = fields[0];
    Constraint c(fname, fields[1]);
    setAERV(fname, c);
  }

  if (!have_input_specs) {
    cerr << "WARNING: EMPTY INPUT SPECS LIST!\n";
  }
}

bool ErrorBlocks::runOnModule(Module &M) {
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
      changed = runOnFunction(*fi) || changed;
    }
  }
  return false;
}

bool ErrorBlocks::runOnFunction(Function &F) {
  bool changed = false;

  // Error values
  for (auto bi = F.begin(), be = F.end(); bi != be; ++bi) {
    BasicBlock &BB = *bi;
    changed = visitBlock(BB) || changed;
  }
  return changed;
}

bool ErrorBlocks::visitBlock(BasicBlock &BB) {
  bool changed = false;

  // Handle calls to error-only functions
  for (auto ii = BB.begin(), ie = BB.end(); ii != ie; ++ii) {
    Instruction &I = *ii;
    if (CallInst *inst = dyn_cast<CallInst>(&I)) {
      changed = visitCallInst(*inst) || changed;
    }
  }
  ReturnConstraints &return_constraints = getAnalysis<ReturnConstraints>();
  ReturnedValues &returned_values = getAnalysis<ReturnedValues>();
  ReturnPropagation &return_propagation = getAnalysis<ReturnPropagation>();

  string parent_fname = BB.getParent()->getName();
  Instruction *bb_first = GetFirstInstructionOfBB(&BB);
  Instruction *bb_last = GetLastInstructionOfBB(&BB);

  ReturnConstraintsFact rcf = return_constraints.getOutFact(bb_last);
  ReturnedValuesFact rtf = returned_values.getInFact(bb_first);
  if (rtf.value.size() > 1)
    return false;

  // Check for error codes
  for (Value *returned_value : rtf.value) {
      if (ConstantInt *int_return = dyn_cast<ConstantInt>(returned_value)) {
        int64_t return_value = int_return->getSExtValue();
        if (error_codes.find(return_value) != error_codes.end()) {
          changed = addErrorValue(BB.getParent(), return_value) || changed;
        }
      }
  }

  // string constraint_fname is the function whose return value is constraining
  // this block
  // Constraint block_constraint is the abstract value of the constraint on
  // block execution
  // Constraint constraint_aerv is the abstract error return value of
  // constraint_f
  for (auto kv : rcf.value) {
    string constraint_fname = kv.first;
    Constraint block_constraint = kv.second;

    // Function constraining this block does not have an AERV (yet)
    // so block constraint can't make it an error block
    if (!haveAERV(constraint_fname)) {
      continue;
    }

    // Get the AERV for function constraining this block
    Constraint constraint_aerv = getAERV(constraint_fname);

    // Check to see if block is executed when function `constraint_fname`
    // returns an error
    if (block_constraint.meet(constraint_aerv).interval != Interval::BOT) {
      // Transform set of values that can be returned into
      // constraint
      //   - bad name here. constraint is interval + join, meet ops
      //   - we are abstracting the set of values into return_interval,
      //     (including propagation)
      for (Value *returned_value : rtf.value) {
        Constraint return_constraint(parent_fname);
        Interval return_interval = Interval::BOT;

        string propagate_callee;
        if (block_constraint.interval != Interval::TOP) {
          if (ConstantInt *int_return = dyn_cast<ConstantInt>(returned_value)) {
            int64_t return_value = int_return->getSExtValue();
            return_interval = abstractInteger(return_value);
            propagate_callee = constraint_fname;
          } else if (isa<ConstantPointerNull>(returned_value)) {
            return_interval = abstractInteger(0);
            propagate_callee = constraint_fname;
          }
        }

        if (isa<CallInst>(returned_value)) {
          // DIRECT PROPAGATION
          // We are returning a call instruction
          // If it is a call instruction,
          // check to see if we have aerv of that function
          CallInst *call = dyn_cast<CallInst>(returned_value);
          string callee_name = getCalleeName(*call);
          if (haveAERV(callee_name)) {
            Constraint callee_aerv = getAERV(callee_name);
            return_interval = callee_aerv.interval;
            propagate_callee = callee_name;
          }
        } else {
          // INDIRECT PROPAGATION
          // We are returning a value which can hold a call instruction at this
          // program point
          // Check to see if returned value can hold return value of function
          ReturnPropagationFact rpf = *return_propagation.getOutputFactAt(bb_last);

          unordered_set<Value*> rpf_rets = rpf.getHeldValues(returned_value);
          if (rpf_rets.size() > 1) continue; 

          for (const auto &v : rpf_rets) {
            if (CallInst *call = dyn_cast<CallInst>(v)) {
              string callee_name = getCalleeName(*call);
              if (haveAERV(callee_name)) {
                Constraint callee_aerv = getAERV(callee_name);
                return_interval = callee_aerv.interval;
                propagate_callee = callee_name;
              }
            }
          }
        }
        return_constraint.interval = return_interval;

        // join abstraction of return value with parent AERV
        if (!haveAERV(parent_fname)) {
          changed = setAERV(parent_fname, return_constraint);
        } else {
          Constraint existing = getAERV(parent_fname);
          changed = setAERV(parent_fname, existing.join(return_constraint));
          changed = changed && (getAERV(parent_fname).interval != existing.interval);
        }
        if (changed && !propagate_callee.empty()) {
            addErrorPropagation(propagate_callee, parent_fname);
        }
      }
    }
  }

  return changed;
}

bool ErrorBlocks::visitCallInst(CallInst &I) {
  // If block contains a call to an error only function
  // Get the set of values that it can return
  // Add those values to error_values
  string callee_name = getCalleeName(I);

  ReturnedValues &returned_values = getAnalysis<ReturnedValues>();

  Function *parent = I.getParent()->getParent();

  bool changed = false;

  // Call to error-only function
  if (error_only.find(callee_name) != error_only.end()) {
    // Get set of values that can be returned from this instruction
    ReturnedValuesFact rtf = returned_values.getOutFact(&I);

    for (const auto &v : rtf.value) {
      if (ConstantInt *int_return = dyn_cast<ConstantInt>(v)) {
        int64_t return_value = int_return->getSExtValue();
        changed = addErrorValue(parent, return_value) || changed;
      } else if (isa<ConstantPointerNull>(v)) {
        changed = addErrorValue(parent, 0) || changed;
      }
    }

    error_only_bootstrap.insert(parent->getName());
  }

  return changed;
}

bool ErrorBlocks::addErrorValue(Function *f, int64_t v) {
  bool changed = false;

  // Insert constant into error_return_values
  if (error_return_values.find(f) == error_return_values.end()) {
    error_return_values[f] = unordered_set<int64_t>({v});
    changed = true;
  } else {
    if (error_return_values[f].find(v) == error_return_values[f].end()) {
      changed = true;
    }
    error_return_values[f].insert(v);
  }

  // Insert abstraction of constant in abstract_error_return_values
  Constraint c(f->getName());
  c.interval = abstractInteger(v);
  string fname = f->getName();
  if (!haveAERV(fname)) {
    abstract_error_return_values[fname] = c;
    changed = true;
  } else {
    Constraint old_aerv = getAERV(fname);
    setAERV(fname, old_aerv.join(c));
    if (getAERV(fname).interval != old_aerv.interval) {
      changed = true;
    }
  }

  //addErrorPropagation(fname, fname);

  return changed;
}

void ErrorBlocks::addErrorPropagation(string from, string to) {
  auto edge = std::make_pair(from, to);
  error_propagation.insert(edge);
}

unordered_map<string, Constraint> ErrorBlocks::getErrorReturnValues() const {
  return abstract_error_return_values;
}

Interval ErrorBlocks::abstractInteger(int64_t v) const {
  if (v < 0) {
    return Interval::LTZ;
  } else if (v > 0) {
    return Interval::GTZ;
  } else {
    return Interval::ZERO;
  }
}

bool ErrorBlocks::haveAERV(string fname) const {
  return abstract_error_return_values.find(fname) !=
         abstract_error_return_values.end();
}

// A constraint object just to use the join function - needs refactor
Constraint ErrorBlocks::getAERV(string fname) const {
  if (abstract_error_return_values.find(fname) ==
      abstract_error_return_values.end()) {
    cerr << "ERROR: no AERV for function " << fname << endl;
    exit(1);
  }

  return abstract_error_return_values.at(fname);
}

bool ErrorBlocks::setAERV(string fname, Constraint c) {
  abstract_error_return_values[fname] = c;
  return true;
}


void ErrorBlocks::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<ReturnPropagation>();
  AU.addRequired<ReturnedValues>();
  AU.addRequired<ReturnConstraints>();
  AU.setPreservesAll();
}

char ErrorBlocks::ID = 0;
static RegisterPass<ErrorBlocks>
    X("errorblocks", "Map each basic block to its error state", false, false);
