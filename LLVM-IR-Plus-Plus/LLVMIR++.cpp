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

enum SymbolType { simple, pointer, arrow, dot, constant, address };

// Declare interface for expression
class Expression {
       public:
	Instruction* base;
	Value* optional;
	Type* type;
	SymbolType symbol;
	bool RHSisAddress;
	virtual void getMetaData(Value*) = 0;
};

// Derive a class for LHS expression
class LHSExpression : public Expression {
       public:
	LHSExpression(Value* Exp) {
		base = nullptr;
		optional = nullptr;
		symbol = simple;
		RHSisAddress = false;
		LLVM_DEBUG(dbgs() << "	Initialize LHS with " << *Exp << "\n";);
		getMetaData(Exp);
		LLVM_DEBUG(dbgs() << "	Base " << base->getName() << "\n";);
		LLVM_DEBUG(dbgs() << "	Meta " << symbol << "\n";);
		LLVM_DEBUG(dbgs() << "	Type " << *type << "\n";);
	}
	void getMetaData(Value* Exp) {
		// check is the instruction of type foo = &bar
		if (Exp->getType()->getTypeID() == 15) {
			RHSisAddress = true;
		}
		Instruction* PreInst = dyn_cast<Instruction>(Exp);
		// check if instruction is of type x = ...
		if (AllocaInst* PreAllocaInst = dyn_cast<AllocaInst>(PreInst)) {
			base = PreInst;
			symbol = simple;
			type = PreAllocaInst->getType();
		} else if (LoadInst* PreLoadInst =
			       dyn_cast<LoadInst>(PreInst)) {
			// check if instruction is of type *X = ...
			Value* RHSPreLoadInst = PreLoadInst->getOperand(0);
			if (AllocaInst* PreAllocaRHSPreLoadInst =
				dyn_cast<AllocaInst>(RHSPreLoadInst)) {
				base = cast<Instruction>(RHSPreLoadInst);
				symbol = pointer;
				type = PreAllocaRHSPreLoadInst->getType();
			}
		} else if (GetElementPtrInst* PreGEPInst =
			       dyn_cast<GetElementPtrInst>(PreInst)) {
			// check if instruction is of type x.f = ... or x -> f =
			// ...
			optional = PreGEPInst;
			Value* RHSPreGEPInst;
			for (auto& op : PreGEPInst->operands()) {
				RHSPreGEPInst = op;
				break;
			}
			Instruction* PreInstRHSPreGEPInst =
			    dyn_cast<Instruction>(RHSPreGEPInst);
			// if instruction is of type x.f = ...
			if (AllocaInst* PreAllocaPreInstRHSPreGEPInst =
				dyn_cast<AllocaInst>(PreInstRHSPreGEPInst)) {
				base = PreInstRHSPreGEPInst;
				symbol = dot;
				type = PreAllocaPreInstRHSPreGEPInst->getType();
			} else {
				// if instruction is of type x -> f = ...
				if (LoadInst* PreLoadPreInstRHSPreGEPInst =
					dyn_cast<LoadInst>(
					    PreInstRHSPreGEPInst)) {
					Value* RHSPreLoadPreInstRHSPreGEPInst =
					    PreLoadPreInstRHSPreGEPInst
						->getOperand(0);
					if (AllocaInst* AllocaPreRHSPreLoadPreInstRHSPreGEPInst =
						dyn_cast<AllocaInst>(
						    RHSPreLoadPreInstRHSPreGEPInst)) {
						base =
						    PreLoadPreInstRHSPreGEPInst;
						type =
						    AllocaPreRHSPreLoadPreInstRHSPreGEPInst
							->getType();
						symbol = arrow;
					}
				}
			}
		}
	}
};

class RHSExpression : public Expression {
       public:
	RHSExpression(Value* Exp) {
		base = nullptr;
		optional = nullptr;
		symbol = simple;
		RHSisAddress = false;
		LLVM_DEBUG(dbgs() << "	Initialize RHS with " << *Exp << "\n";);
		getMetaData(Exp);
	}
	void getMetaData(Value* Exp) {
		if (Exp->getType()->getTypeID() == 15) {
			RHSisAddress = true;
			base = cast<Instruction>(Exp);
			return;
		}
		if (ConstantInt* CI = dyn_cast<ConstantInt>(Exp)) {
			symbol = constant;
			return;
		}
		Value* RHSPreLoadInst = cast<LoadInst>(Exp)->getOperand(0);
		Instruction* PreInst = dyn_cast<Instruction>(RHSPreLoadInst);
		// check if instruction is of type x = ...
		if (AllocaInst* PreAllocaInst = dyn_cast<AllocaInst>(PreInst)) {
			base = PreInst;
			symbol = simple;
			type = PreAllocaInst->getType();
		} else if (LoadInst* PreLoadInst =
			       dyn_cast<LoadInst>(PreInst)) {
			// check if instruction is of type *X = ...
			Value* RHSPreLoadInst = PreLoadInst->getOperand(0);
			if (AllocaInst* PreAllocaRHSPreLoadInst =
				dyn_cast<AllocaInst>(RHSPreLoadInst)) {
				base = cast<Instruction>(RHSPreLoadInst);
				symbol = pointer;
				type = PreAllocaRHSPreLoadInst->getType();
			}
		} else if (GetElementPtrInst* PreGEPInst =
			       dyn_cast<GetElementPtrInst>(PreInst)) {
			// check if instruction is of type x.f = ... or x -> f =
			// ...
			optional = PreGEPInst;
			Value* RHSPreGEPInst;
			for (auto& op : PreGEPInst->operands()) {
				RHSPreGEPInst = op;
				break;
			}
			Instruction* PreInstRHSPreGEPInst =
			    dyn_cast<Instruction>(RHSPreGEPInst);
			// if instruction is of type x.f = ...
			if (AllocaInst* PreAllocaPreInstRHSPreGEPInst =
				dyn_cast<AllocaInst>(PreInstRHSPreGEPInst)) {
				base = PreInstRHSPreGEPInst;
				symbol = dot;
				type = PreAllocaPreInstRHSPreGEPInst->getType();
			} else {
				// if instruction is of type x -> f = ...
				if (LoadInst* PreLoadPreInstRHSPreGEPInst =
					dyn_cast<LoadInst>(
					    PreInstRHSPreGEPInst)) {
					Value* RHSPreLoadPreInstRHSPreGEPInst =
					    PreLoadPreInstRHSPreGEPInst
						->getOperand(0);
					if (AllocaInst* AllocaPreRHSPreLoadPreInstRHSPreGEPInst =
						dyn_cast<AllocaInst>(
						    RHSPreLoadPreInstRHSPreGEPInst)) {
						base =
						    PreLoadPreInstRHSPreGEPInst;
						type =
						    AllocaPreRHSPreLoadPreInstRHSPreGEPInst
							->getType();
						symbol = arrow;
					}
				}
			}
		}
	}
};

class UpdateInst {
       public:
	Expression *LHS, *RHS;
	UpdateInst(StoreInst* I) {
		LLVM_DEBUG(dbgs() << " 	UpdateI object initialized with " << *I
				  << "\n";);
		// Get the LHS value of the store instruction
		Value* StoreInstLHS = I->getPointerOperand();
		// Generate LHS meta data
		LHS = new LHSExpression(StoreInstLHS);
		// Get the RHS value of the store instruction
		Value* StoreInstRHS = I->getValueOperand();
		// Generate RHS meta data
		RHS = new RHSExpression(StoreInstRHS);
	}
};

using InstMetaMap = std::map<StoreInst*, UpdateInst*>;

class LLVMIRPlusPlusPass : public FunctionPass {
       private:
	InstMetaMap IRPlusPlus;

       public:
	static char ID;
	LLVMIRPlusPlusPass() : FunctionPass(ID) {}

	bool runOnFunction(Function& F) override {
		// Iterate over basicblocks
		for (BasicBlock& BB : F) {
			// Iterate over Instructions
			for (Instruction& I : BB) {
				// For every store instruction
				if (StoreInst* StoreI =
					dyn_cast<StoreInst>(&I)) {
					LLVM_DEBUG(dbgs() << I << "\n";);
					// Generate meta-data for store
					// instruction
					UpdateInst* UpdateI =
					    new UpdateInst(StoreI);
					IRPlusPlus[StoreI] = UpdateI;
					Expression* L = UpdateI->LHS;
					printExp(L);
					LLVM_DEBUG(dbgs() << " = ";);
					Expression* R = UpdateI->RHS;
					if (R->RHSisAddress & L->RHSisAddress) {
						LLVM_DEBUG(dbgs() << " &";);
						R->symbol = address;
					}
					printExp(R);
					LLVM_DEBUG(dbgs() << "\n";);

					delete UpdateI;
					UpdateI = nullptr;
				}
			}
		}
		return false;
	}
	void printExp(Expression* L) {
		if (L->symbol == constant) {
			LLVM_DEBUG(dbgs() << "constant";);
			return;
		}
		if (L->symbol != address) {
			LLVM_DEBUG(dbgs() << *(L->type) << " ";);
		}
		if (L->symbol == pointer) {
			LLVM_DEBUG(dbgs() << "*";);
		}
		if (L->base) {
			LLVM_DEBUG(dbgs() << L->base->getName() << " ";);
		}
		if (L->symbol == arrow) {
			LLVM_DEBUG(dbgs() << " -> ";);
		}
		if (L->symbol == dot) {
			LLVM_DEBUG(dbgs() << " . ";);
		}
		if (L->optional) {
			LLVM_DEBUG(dbgs() << L->optional->getName(););
		}
	}

	InstMetaMap getIRPlusPlus() { return IRPlusPlus; }
};
}  // namespace

char LLVMIRPlusPlusPass::ID = 0;
static RegisterPass<LLVMIRPlusPlusPass> X(
    "llvmir++",					     // the option name -> -mba
    "LLVM IR abstracted to C++ three address code",  // option description
    true,  // true as we don't modify the CFG
    true   // true if we're writing an analysis
);
/*
static void registerLLVMIRPlusPlusPass(const PassManagerBuilder &,
			     legacy::PassManagerBase &PM) {
	PM.add(new LLVMIRPlusPlusPass());
}
static RegisterStandardPasses RegisterMyPass(
    PassManagerBuilder::EP_EarlyAsPossible, registerLLVMIRPlusPlusPass);
*/
