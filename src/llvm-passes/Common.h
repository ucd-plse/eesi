#ifndef COMMON_H
#define COMMON_H

#include "llvm/IR/Instructions.h"
#include "Constraint.h"
#include <string>

namespace errspec {

std::string getCalleeName(const llvm::CallInst &I);

llvm::Instruction *GetFirstInstructionOfBB(llvm::BasicBlock *bb);
llvm::Instruction *GetLastInstructionOfBB(llvm::BasicBlock *bb);
std::pair<Interval, Interval> abstractICmp(llvm::ICmpInst &I);
}

#endif