#ifndef LLVMIR___H
#define LLVMIR___H

#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <utility>
#include <unordered_set>
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
#include "llvm/IR/Module.h"
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

enum SymbolType { simple, pointer, arrow, dot, constant, address };

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
	// Generally stores the alloca instruction for a variable eg a
	Instruction* base;
	// Saves the offset address eg a -> f or a.f, &f is saved in optional
	Value* optional;
	// Type of the variable
	Type* type;
	// Can be one of the six symbol types
	SymbolType symbol;
	// Exceptions to alloca instruction can be global variable and function
	// arguments this data member saved them separately
	Value* functionArg;
	// For RHS of type &x this data member is marked true
	bool RHSisAddress;
	// resets the metadata to init values
	void resetMetadata();
	// If the Value* is global it returns true and handles LHSExpression or
	// RHSExpression object
	bool handleGlobalVariable(Value*);
	Expression();
	Expression(const Expression*);
	class ExpEqual {
	       public:
		bool operator()(Expression const*, Expression const*) const;
	};
};

/* LHSExpression
 * Store LHS for any assignment
 * LHS = ...
 */
class LHSExpression : public Expression {
       public:
	LHSExpression(Value*);
	void getMetaData(Value*);
};

/* RHSExpression
 * Store RHS for any assignment
 * ... = RHS
 */
class RHSExpression : public Expression {
       public:
	RHSExpression(Value*);
	void getMetaData(Value*);
};

/* UpdateInst
 * Store assignment
 * LHS = RHS
 */
class UpdateInst {
       public:
	Expression *LHS, *RHS;
	UpdateInst(StoreInst* I);
};

/* Maps store instruction to the meta data */
using InstMetaMap = std::map<StoreInst*, UpdateInst*>;
class Node;
using NodeList = std::vector<Node*>;
InstMetaMap IRPlusPlus;


class Node {
	private:
	Instruction* Inst;
	Expression* LHS;
	Expression* RHS;
	bool isAbstracted;
	int loc;
	NodeList Succ, Pred;
	public:
	void resetNode(){
		Inst = nullptr;
		LHS = nullptr;
		RHS = nullptr;
		isAbstracted = false;
		loc = -1;
	}
	NodeList getSucc(){
		NodeList ans;
		NodeList WorkList;
		WorkList = getRealSucc();
		while(!WorkList.empty()){
			Node* temp = WorkList.back();
			WorkList.pop_back();
			if(temp -> isAbstracted == true){
				ans.push_back(temp);
			} else {
				std::vector<Node*> s = temp -> getRealSucc();
				for(Node* n : s){
					WorkList.push_back(n);
				}
			}
		}
		return ans;
	}
	NodeList getRealSucc(){
		return Succ;
	}
	NodeList getRealPred(){
		return Pred;
	}
	Node(){
		resetNode();
	}
	Node(Instruction*);
};

class CFG {
	private:
	Node* StartNode;
	Node* EndNode;
	public:
	CFG(){
		StartNode = nullptr;
		EndNode = nullptr;
	}
	void init(Module* M){
		if(StartNode){
			// The cgf is already inited
			return;
		}
		for(Function& F : *M){
			for(BasicBlock& BB : F){
				for(Instruction& I : BB){
					Node* tempNode = new Node(&I);
					if(!StartNode){
						StartNode = tempNode;
					}
					NodeList EndInstList;
					NodeList WorkList;
					WorkList.push_back(StartNode);
					while(! WorkList.empty()){
						Node* temp = WorkList.back();
						WorkList.pop_back();
						if((temp -> getRealSucc()).size() == 0){
							EndInstList.push_back(temp);
						}
						for(Node* s : temp -> getRealSucc()){
							WorkList.push_back(s);
						}
					}
					Node* endNode;
					for(Node* end : WorkList){
						(end -> getRealSucc()).push_back(endNode);
						(endNode -> getRealPred()).push_back(end);
					}
					break;
				}
			}
		}
	}
};

using CFGMap = std::map<Module*, CFG*>;

class LLVMIRPlusPlusPass : public ModulePass {
       public:
	CFG* grcfg;
	static char ID;
	LLVMIRPlusPlusPass();
	bool runOnModule(Module&) override;
	// prints expression
	void printExp(Expression*);
	// returns generated metadata
	InstMetaMap getIRPlusPlus();
	CFG* getCFG(){
		return grcfg;
	}
	// force generate metadata for one store instruction
	void generateMetaData(StoreInst*);
	// returns CFG
};

#endif
