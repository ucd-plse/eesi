#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"
#include <iostream>
#include <string>
#include "DefinedFunctions.h"

using namespace llvm;
using namespace std;

bool DefinedFunctions::runOnFunction(Function &F) {
    // This is a function pass not a module pass.
    // we don't need to explicitly strip out intrinsics or declarations.

    string fname = F.getName().str();
    auto idx = fname.find('.');
    if (idx != string::npos) {
      fname = fname.substr(0, idx);
    }
    defined_functions.insert(fname);

    F.getReturnType()->print(llvm::outs());
    llvm::outs() << " " << fname << "\n";

    // Works in LLVM 3.8
    // -------------------------
    // Get the file that defines the function
    // DISubprogram *subprogram = F.getSubprogram();
    // if (subprogram) {
    //   DefinedFunction func;
    //   func.name = fname;
    //   func.file = subprogram->getFilename();
    //   defined_functions.push_back(func);
    // }

    return false;
}

std::unordered_set<std::string> DefinedFunctions::getDefinedFunctions() {
  return defined_functions;
}


char DefinedFunctions::ID = 0;
static RegisterPass<DefinedFunctions> X("definedfunctions",
                                        "List functions defined in a bitcode file",
                                        false, false);
