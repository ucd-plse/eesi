#include <iostream>
#include <string>
#include <unordered_set>

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"
#include "CalledFunctions.h"
#include "Common.h"

using namespace llvm;
using namespace std;
using namespace errspec;

bool CalledFunctions::runOnFunction(Function &F) {
  // This is a function pass not a module pass.
  // we don't need to explicitly strip out intrinsics or declarations.

  for (auto bi = F.begin(), be = F.end(); bi != be; ++bi) {
    BasicBlock *bb = &*bi;
    for (auto ii = bb->begin(), ie = bb->end(); ii != ie; ++ii) {
      Instruction *i = &*ii;

      if (CallInst *call = dyn_cast<CallInst>(i)) {
        string fname = getCalleeName(*call);
        cout << fname << endl;
      }
    }
  }

  return false;
}

std::unordered_set<string> CalledFunctions::getCalledFunctions() {
  return called_functions;
}

void CalledFunctions::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

char CalledFunctions::ID = 0;
static RegisterPass<CalledFunctions> X("calledfunctions",
  "List functions called in a bitcode file", false, false);
