#ifndef LLVMIR___H 
#define LLVMIR___H 

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
#include "llvm/IR/Module.h"
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

enum SymbolType { simple, pointer, arrow, dot, constant, address};

/* Statement 	:= LHS = RHS
 * LHS		:= x | *x | x -> f | x.f
 * RHS		:= y | *y | y -> f | y.f | &y
 */

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
//	virtual void getMetaData(Value*) = 0;
	void resetMetadata();
	bool handleGlobalVariable(Value*);
	Expression();
	Expression(const Expression*);
	class ExpEqual{
	public:
		bool operator()(Expression const *,Expression const *) const;
};


};

class LHSExpression : public Expression {
       public:
	LHSExpression(Value*);
	void getMetaData(Value*);
};

class RHSExpression : public Expression {
       public:
	RHSExpression(Value*);
	void getMetaData(Value*);
};

class UpdateInst {
       public:
	Expression *LHS, *RHS;
	UpdateInst(StoreInst* I);
};

using InstMetaMap = std::map<StoreInst*, UpdateInst*>;

class LLVMIRPlusPlusPass : public ModulePass {
       private:
	InstMetaMap IRPlusPlus;

       public:
	static char ID;
	LLVMIRPlusPlusPass();
	bool runOnModule(Module&) override;
	void printExp(Expression*);
	InstMetaMap getIRPlusPlus();
	void generateMetaData(StoreInst*);
};
#endif 
