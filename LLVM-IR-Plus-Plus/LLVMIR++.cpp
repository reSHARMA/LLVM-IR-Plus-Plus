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

/* Statement 	:= LHS = RHS
 * LHS		:= x | *x | x -> f | x.f
 * RHS		:= y | *y | y -> f | y.f | &y
 */

enum SymbolType { simple, pointer, arrow, dot, constant, address };

/* Symbol Type 	Variable pattern
 * simple	x
 * pointer	*x
 * arrow	x -> f
 * dot		x.f
 * constant	0x3546
 * address	&x
 */

// Declare interface for expression

/* for LHS Expression x -> f where x is of type of Class.A*
 * base 	x
 * type		Class.A*
 * symbol	arrow
 */

/* for RHS Expression &x where x is of type Class.A*
 * base 	x
 * type		Class.A*
 * RHSisAddess	true
 */

class Expression {
       public:
	Instruction* base;
	Value* optional;
	Type* type;
	SymbolType symbol;
	Value* functionArg;
	bool RHSisAddress;
	virtual void getMetaData(Value*) = 0;
	void resetMetadata();
	bool handleGlobalVariable(Value*);
};

void Expression::resetMetadata() {
	base = nullptr;
	optional = nullptr;
	type = nullptr;
	symbol = simple;
	functionArg = nullptr;
	RHSisAddress = false;
}

bool Expression::handleGlobalVariable(Value* Exp) {
	if (Exp && isa<GlobalVariable>(Exp)) {
		functionArg = Exp;
		type = Exp->getType();
		symbol = simple;
		return true;
	}
	return false;
}

// Derive a class for LHS expression
class LHSExpression : public Expression {
       public:
	LHSExpression(Value* Exp) {
		resetMetadata();
		LLVM_DEBUG(dbgs() << "	Initialize LHS with " << *Exp << "\n";);
		getMetaData(Exp);
	}
	void getMetaData(Value* Exp) {
		// update for variable being a global variable
		if (handleGlobalVariable(Exp)) return;
		// handles pointer type
		if (Exp->getType()->getTypeID() == 15) {
			RHSisAddress = true;
		}
		if (Instruction* PreInst = dyn_cast<Instruction>(Exp)) {
			// check if instruction is of type x = ...
			if (AllocaInst* PreAllocaInst =
				dyn_cast<AllocaInst>(PreInst)) {
				base = PreInst;
				symbol = simple;
				type = PreAllocaInst->getType();
			} else if (LoadInst* PreLoadInst =
				       dyn_cast<LoadInst>(PreInst)) {
				// check if instruction is of type *X = ...
				Value* RHSPreLoadInst =
				    PreLoadInst->getOperand(0);
				if (AllocaInst* PreAllocaRHSPreLoadInst =
					dyn_cast<AllocaInst>(RHSPreLoadInst)) {
					base =
					    cast<Instruction>(RHSPreLoadInst);
					symbol = pointer;
					type =
					    PreAllocaRHSPreLoadInst->getType();
				}
			} else if (GetElementPtrInst* PreGEPInst =
				       dyn_cast<GetElementPtrInst>(PreInst)) {
				// check if instruction is of type x.f = ... or
				// x -> f =
				// ...
				optional = PreGEPInst;
				Value* RHSPreGEPInst;
				for (auto& op : PreGEPInst->operands()) {
					RHSPreGEPInst = op;
					break;
				}
				if (Instruction* PreInstRHSPreGEPInst =
					dyn_cast<Instruction>(RHSPreGEPInst)) {
					// if instruction is of type x.f = ...
					if (AllocaInst* PreAllocaPreInstRHSPreGEPInst =
						dyn_cast<AllocaInst>(
						    PreInstRHSPreGEPInst)) {
						base = PreInstRHSPreGEPInst;
						symbol = dot;
						type =
						    PreAllocaPreInstRHSPreGEPInst
							->getType();
					} else {
						// if instruction is of type x
						// -> f = ...
						if (LoadInst* PreLoadPreInstRHSPreGEPInst =
							dyn_cast<LoadInst>(
							    PreInstRHSPreGEPInst)) {
							Value*
							    RHSPreLoadPreInstRHSPreGEPInst =
								PreLoadPreInstRHSPreGEPInst
								    ->getOperand(
									0);
							if (AllocaInst* AllocaPreRHSPreLoadPreInstRHSPreGEPInst =
								dyn_cast<
								    AllocaInst>(
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
		}
		if (!base) {
			// variable is probably a function parameter
			base = dyn_cast<Instruction>(Exp);
			functionArg = Exp;
			type = Exp->getType();
		}
		if (!type) {
			Exp->getType();
		}
	}
};

class RHSExpression : public Expression {
       public:
	RHSExpression(Value* Exp) {
		resetMetadata();
		LLVM_DEBUG(dbgs() << "	Initialize RHS with " << *Exp << "\n";);
		getMetaData(Exp);
	}
	void getMetaData(Value* Exp) {
		// If the variable is a global variable use it
		if (handleGlobalVariable(Exp)) return;
		if (Exp && isa<BitCastInst>(Exp)){
			BitCastInst* bitcastInst = dyn_cast<BitCastInst>(Exp);
			Exp = bitcastInst -> getOperand(0);
			LLVM_DEBUG(dbgs() << "	BitCast Instruction: " << *bitcastInst << "\n";);
			LLVM_DEBUG(dbgs() << "	New RHS: " << *Exp << "\n";);
		}
		// Check if the expression is of pointer type
		if (Exp->getType()->getTypeID() == 15) {
			LLVM_DEBUG(dbgs() << "	RHS is of pointer type \n";);
			RHSisAddress = true;
			base = dyn_cast<Instruction>(Exp);
			if (!base) {
				// The variable is not defined in the current
				// function ie can be a parameter
				functionArg = Exp;
			}
			type = Exp->getType();
		} 
		if (Constant* CI = dyn_cast<Constant>(Exp)) {
			LLVM_DEBUG(dbgs() << "Replaced constant " << *CI << "\n";);
			// replace calls with constant since this is a
			// context-insensitive analysis
			symbol = constant;
		} else if (AllocaInst* PreAllocaInst =
			       dyn_cast<AllocaInst>(Exp)) {
			LLVM_DEBUG(dbgs() << "Found alloca: " << *PreAllocaInst << "\n";);
			// check if the variable is a plain declared variable ie
			// int x = ...
			base = dyn_cast<Instruction>(Exp);
			// TODO: Do I need to check if it is a global/function
			// argument if it a alloca is found?
			if (!base) {
				functionArg = Exp;
			}
			symbol = simple;
			type = PreAllocaInst->getType();
		} else if (isa<LoadInst>(Exp)) {
			LLVM_DEBUG(dbgs() << "Found load : " << *Exp << "\n";);
			// if the previous instruction is of type load then
			// following cases are possible x = ... *x = ... x.f =
			// ... x -> f = ...
			Value* RHSPreLoadInst =
			    cast<LoadInst>(Exp)->getOperand(0);
			Instruction* PreInst =
			    dyn_cast<Instruction>(RHSPreLoadInst);
			// check if instruction is of type x = ...
			if (AllocaInst* PreAllocaInst =
				dyn_cast<AllocaInst>(PreInst)) {
				base = PreInst;
				symbol = simple;
				type = PreAllocaInst->getType();
			} else if (LoadInst* PreLoadInst =
				       dyn_cast<LoadInst>(PreInst)) {
				// check if instruction is of type *X = ...
				Value* RHSPreLoadInst =
				    PreLoadInst->getOperand(0);
				if (AllocaInst* PreAllocaRHSPreLoadInst =
					dyn_cast<AllocaInst>(RHSPreLoadInst)) {
					base =
					    cast<Instruction>(RHSPreLoadInst);
					symbol = pointer;
					type =
					    PreAllocaRHSPreLoadInst->getType();
				}
			} else if (GetElementPtrInst* PreGEPInst =
				       dyn_cast<GetElementPtrInst>(PreInst)) {
				// check if instruction is of type x.f = ... or
				// x -> f =
				// ...
				optional = PreGEPInst;
				Value* RHSPreGEPInst;
				for (auto& op :
				     cast<User>(PreGEPInst)->operands()) {
					RHSPreGEPInst = op;
					break;
				}
				Instruction* PreInstRHSPreGEPInst =
				    dyn_cast<Instruction>(RHSPreGEPInst);
				// if instruction is of type x.f = ...
				if (AllocaInst* PreAllocaPreInstRHSPreGEPInst =
					dyn_cast<AllocaInst>(
					    PreInstRHSPreGEPInst)) {
					base = PreInstRHSPreGEPInst;
					symbol = dot;
					type = PreAllocaPreInstRHSPreGEPInst
						   ->getType();
				} else {
					// if instruction is of type x -> f =
					// ...
					if (LoadInst* PreLoadPreInstRHSPreGEPInst =
						dyn_cast<LoadInst>(
						    PreInstRHSPreGEPInst)) {
						Value*
						    RHSPreLoadPreInstRHSPreGEPInst =
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
		} else if (CallInst* CI = dyn_cast<CallInst>(Exp)) {
			LLVM_DEBUG(dbgs() << "Replace call " << *CI << "with constant" << "\n";);
			// replace call with constant
			symbol = constant;
		} else if (isa<User>(Exp)) {
			// if we come across any other other pattern then we
			// utilize commutative property of some operations
			// example LHS = RHS then type of LHS and RHS are needed
			// to be same LHS = RHS1 + RHS2 + ... then type of LHS
			// and RHSi would be same
			for (auto& op : cast<User>(Exp)->operands()) {
				getMetaData(op);
			}
		} else {
			// function parameters used directly
			base = dyn_cast<Instruction>(Exp);
			functionArg = Exp;
			type = Exp->getType();
		}
	}
};

class UpdateInst {
       public:
	Expression *LHS, *RHS;
	UpdateInst(StoreInst* I) {
		LLVM_DEBUG(dbgs() << " 	UpdateI object initialized with " << *I << "\n";);
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
	// store data in map that can be used in other analysis/transform
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
				}
			}
		}
		return false;
	}
	void printExp(Expression* L) {
		// prints the expression in formal LHS = RHS
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
		} else if (L->functionArg) {
			LLVM_DEBUG(dbgs() << L->functionArg->getName() << " ";);
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
