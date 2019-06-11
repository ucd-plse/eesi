#include <iostream>
#include <fstream>
#include <unordered_set>

#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/LLVMContext.h"
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <glog/logging.h>

#include "Constraint.h"
#include "DefinedFunctions.h"
#include "ErrorBlocks.h"
#include "ReturnPropagation.h"
#include "ReturnPropagationPointer.h"
#include "ReturnConstraints.h"
#include "ReturnConstraintsPointer.h"
#include "ReturnedValues.h"
#include "CalledFunctions.h"
#include "MissingChecks.h"

using namespace std;
using namespace llvm;

// commands
void calledfunctions(Module &Mod);
void definedfunctions(Module &Mod);
void bugs(Module &Mod, string specs_path, string error_only_path, string debug_function);
void errorpropagation(Module &Mod, string error_only_path,
                      string input_specs_path);
void fullpropagation(Module &Mod, string error_only_path);
void specs(Module &Mod, string error_only_path, string input_specs_path);

int main(int argc, char **argv) {
  namespace po = boost::program_options;
  po::options_description desc("Options");
  desc.add_options()("help", "produce help message")
      ("bitcode", po::value<string>()->required(), "Path to bitcode file")
      ("command", po::value<string>()->required(), "Command (See README)")
      ("output", po::value<string>(), "Path to output file")
      ("erroronly", po::value<string>(), "Path to error-only functions file")
      ("inputspecs", po::value<string>(), "Path to input specs list file")
      ("specs", po::value<string>(), "Path to specs file")
      ("debugfunction", po::value<string>(), "Print log messages when processing this function");
  po::variables_map varmap;
  try {
    po::store(po::parse_command_line(argc, argv, desc), varmap);

    // Deal with --help before notify to avoid error
    // about missing required arguments
    if (varmap.count("help")) {
      std::cout << desc << "\n";
      return 0;
    }

    po::notify(varmap);
  } catch (po::error &e) {
    cerr << "ERROR: " << e.what() << endl << endl;
    cerr << desc << endl;
    return 1;
  }

  unordered_set<string> valid_commands = {"specs",
                                          "handlers",
                                          "errdb",
                                          "definedfunctions",
                                          "calledfunctions",
                                          "bugs",
                                          "errorpropagation",
                                          "fullpropagation",
                                          "usagespecs"};

  string command = varmap["command"].as<string>();
  if (valid_commands.find(command) == valid_commands.end()) {
    cerr << "Command must be one of: " << endl;
    for (auto const &cmd : valid_commands) {
      cerr << cmd << endl;
    }
    return 1;
  }

  string bitcode_path = varmap["bitcode"].as<string>();
  string function;
  if (varmap.count("function")) {
    function = varmap["function"].as<string>();
  }
  string output;
  if (varmap.count("output")) {
    output = varmap["output"].as<string>();
  }

  string error_only_path;
  if (varmap.count("erroronly")) {
    error_only_path = varmap["erroronly"].as<string>();
  }

  string input_specs_path;
  if (varmap.count("inputspecs")) {
    input_specs_path = varmap["inputspecs"].as<string>();
  }

  string specs_path;
  if (varmap.count("specs")) {
    specs_path = varmap["specs"].as<string>();
  }

  string debug_function;
  if (varmap.count("debugfunction")) {
    debug_function = varmap["debugfunction"].as<string>();
  }

  google::InitGoogleLogging(argv[0]);

  SMDiagnostic Err;
  LLVMContext Context;
  unique_ptr<Module> Mod(parseIRFile(bitcode_path, Err, Context));
  if (!Mod) {
    cerr << "FATAL: Error parsing bitcode file: " << bitcode_path << endl;
    abort();
  }

  if (command == "specs") {
    specs(*Mod, error_only_path, input_specs_path);
  } else if (command == "bugs") {
    bugs(*Mod, specs_path, error_only_path, debug_function);
  } else if (command == "definedfunctions") {
    definedfunctions(*Mod);
  } else if (command == "calledfunctions") {
    calledfunctions(*Mod);
  } else if (command == "errorpropagation") {
    errorpropagation(*Mod, error_only_path, input_specs_path);
  } else if (command == "fullpropagation") {
    fullpropagation(*Mod, error_only_path);
  } 

  return 0;
}

void fullpropagation(Module &Mod, string error_only_path) {
  legacy::PassManager PM;
  ReturnPropagation *return_propagation = new ReturnPropagation();
  ReturnConstraints *return_constraints = new ReturnConstraints();
  ErrorBlocks *error_blocks = new ErrorBlocks(error_only_path);
  ReturnedValues *returned_values = new ReturnedValues();
  PM.add(return_propagation);
  PM.add(return_constraints);
  PM.add(returned_values);
  PM.run(Mod);

  unordered_map<string, Constraint> propagated_specs;

  unordered_map<llvm::Function *, unordered_set<string>> return_propagated =
      returned_values->getReturnPropagation();

  cout << "digraph full_prop {\n";
  for (const auto &rp : return_propagated) {
    llvm::Function *f = rp.first;
    string fname = f->getName();

    Constraint fc(fname);
    fc.interval = Interval::BOT;
    if (propagated_specs.find(fname) != propagated_specs.end()) {
      fc = propagated_specs.at(fname);
    }

    for (const auto &v : rp.second) {
      Constraint vc;
      vc.interval = Interval::BOT;
      if (propagated_specs.find(v) != propagated_specs.end()) {
        vc = propagated_specs.at(v);
      }

      if (fname.find(".") != string::npos) {
        continue;
      }
      if (v.find(".") != string::npos) {
        continue;
      }

      cout << "\"" << v << "(" << vc.interval << ")\" -> \"" << fname << "("
           << fc.interval << ")\""
           << "\n";
    }
  }
  cout << "}\n";
}

void specs(Module &Mod, string error_only_path, string input_specs_path) {
  legacy::PassManager PM;
  ReturnPropagation *return_propagation = new ReturnPropagation();
  ReturnConstraints *return_constraints = new ReturnConstraints();
  ErrorBlocks *error_blocks =
      new ErrorBlocks(error_only_path, input_specs_path);
  ReturnedValues *returned_values = new ReturnedValues();
  PM.add(return_propagation);
  PM.add(return_constraints);
  PM.add(error_blocks);
  PM.add(returned_values);
  PM.run(Mod);

  unordered_map<string, Constraint> abstract_error_return_values =
      error_blocks->getErrorReturnValues();

  // Print specs
  for (const auto &kv : abstract_error_return_values) {
    string fname = kv.first;
    Constraint aerv = kv.second;
    cout << fname << ": " << aerv;
    cout << endl;
  }
}

// The constant values that each function can return
void errorpropagation(Module &Mod, string error_only_path,
                      string input_specs_path) {
  legacy::PassManager PM;
  ReturnPropagation *return_propagation = new ReturnPropagation();
  ReturnConstraints *return_constraints = new ReturnConstraints();
  ErrorBlocks *error_blocks =
      new ErrorBlocks(error_only_path, input_specs_path);
  ReturnedValues *returned_values = new ReturnedValues();
  PM.add(return_propagation);
  PM.add(return_constraints);
  PM.add(error_blocks);
  PM.add(returned_values);
  PM.run(Mod);

  // Returned values
  unordered_map<string, Constraint> abstract_error_return_values =
      error_blocks->getErrorReturnValues();

  // Print error propagation graph
  cout << "digraph error_prop {\n";
  for (const auto &erp : error_blocks->error_propagation) {
    string from_name = erp.first;
    string to_name = erp.second;
    auto &bootstrap = error_blocks->error_only_bootstrap;
    Constraint from_spec = error_blocks->getAERV(from_name);
    Constraint to_spec = error_blocks->getAERV(to_name);
    if (bootstrap.find(from_name) != bootstrap.end()) {
      from_name = from_name + "(EO)";
    }
    if (bootstrap.find(to_name) != bootstrap.end()) {
      to_name = to_name + "(EO)";
    }
    cout << "\"" << from_name << " " << from_spec.interval << "\""
         << " -> \"" << to_name << " " << to_spec.interval << "\""
         << "\n";
  }
  cout << "}\n";

  return;
}

void bugs(Module &Mod, string specs_path, string error_only_path, string debug_function) {
  legacy::PassManager PM;

  ReturnPropagationPointer *return_propagation = new ReturnPropagationPointer(debug_function);
  ReturnConstraintsPointer *return_constraints = new ReturnConstraintsPointer;
  MissingChecks *missing_checks = new MissingChecks(specs_path, error_only_path, debug_function);

  PM.add(return_propagation);
  PM.add(return_constraints);
  PM.add(missing_checks);
  PM.run(Mod);
}

// Prints a list of functions defined in the bitcode file
void definedfunctions(Module &Mod) {
  legacy::PassManager PM;
  DefinedFunctions *defined_functions = new DefinedFunctions;
  PM.add(defined_functions);
  PM.run(Mod);
}

void calledfunctions(Module &Mod) {
  legacy::PassManager PM;
  CalledFunctions *called_functions = new CalledFunctions;
  PM.add(called_functions);
  PM.run(Mod);
}
