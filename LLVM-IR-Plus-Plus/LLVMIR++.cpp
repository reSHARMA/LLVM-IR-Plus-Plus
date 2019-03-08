#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

namespace {

#define DEBUG_TYPE "llvmir++"

class LLVMIRPlusPlusPass : public FunctionPass {
       public:
	static char ID;
	LLVMIRPlusPlusPass() : FunctionPass(ID) {}

	bool runOnFunction(Function &F) override {
		// 
		return false;
	}
};
}  // namespace

char LLVMIRPlusPlusPass::ID = 0;
static RegisterPass<LLVMIRPlusPlusPass> X(
    "llvmir++",			      // the option name -> -mba
    "LLVM IR abstracted to C++ three address code",  // option description
    true,			      // true as we don't modify the CFG
    true			      // true if we're writing an analysis
);
/*
static void registerLLVMIRPlusPlusPass(const PassManagerBuilder &,
			     legacy::PassManagerBase &PM) {
	PM.add(new LLVMIRPlusPlusPass());
}
static RegisterStandardPasses RegisterMyPass(
    PassManagerBuilder::EP_EarlyAsPossible, registerLLVMIRPlusPlusPass);
*/
