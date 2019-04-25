#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>
#include "include/LLVMIR++.h"
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

#define DEBUG_TYPE "llvmir++"

/* resetMetadata
 * Reset all Expression class member to their initial value
 */
void Expression::resetMetadata() {
	base = nullptr;
	optional = nullptr;
	type = nullptr;
	symbol = simple;
	functionArg = nullptr;
	RHSisAddress = false;
}

/* handleGlobalVariable
 * It is possible that the defination of global variable is
 * not present in the current file so instead of assigning it
 * to the base we assign it to functionArg.
 *
 * handleGlobaleVariable takes Value* as an input
 * It returns TRUE if the value was a global expression
 * and will update the type, functionArg and symbol.
 * else it returns false
 */
bool Expression::handleGlobalVariable(Value* Exp) {
	if (Exp && isa<GlobalVariable>(Exp)) {
		functionArg = Exp;
		type = Exp->getType();
		symbol = simple;
		return true;
	}
	return false;
}

/* Copy constructor */
Expression::Expression(const Expression* Exp) {
	base = Exp->base;
	optional = Exp->optional;
	type = Exp->type;
	symbol = Exp->symbol;
	functionArg = Exp->functionArg;
	RHSisAddress = Exp->RHSisAddress;
}

/* Default constructor */
Expression::Expression() { resetMetadata(); }

/* ExpEqual
 * Matches all the data member of two Expression class object and returns a
 * boolean value
 */
bool Expression::ExpEqual::operator()(Expression const* exp1,
				      Expression const* exp2) const {
	if (exp1->base != exp2->base) {
		return false;
	}
	if (exp1->type != exp2->type) {
		return false;
	}
	if (exp1->symbol != exp2->symbol) {
		return false;
	}
	if (exp1->functionArg != exp2->functionArg) {
		return false;
	}
	if (exp1->RHSisAddress != exp2->RHSisAddress) {
		return false;
	}
	if (exp1->optional != exp2->optional) {
		return false;
	}
	return true;
}

/* Constructor for LHSExpression
 * For an input Value* Exp it generates a LHSExpression object for it
 * It resets meta data and uses getMetaData to abstract meta data
 */
LHSExpression::LHSExpression(Value* Exp) {
	resetMetadata();
	LLVM_DEBUG(dbgs() << "	Initialize LHS with " << *Exp << "\n";);
	getMetaData(Exp);
}

/* getMetaData
 * It extracts metadata from Value* Exp using some fixed pattern in the IR
 */
void LHSExpression::getMetaData(Value* Exp) {
	// update for variable being a global variable
	if (handleGlobalVariable(Exp)) return;
	// Handle bitcast
	// For a = (new type) b, Exp is a
	// change Exp to b and move on
	if (Exp && isa<BitCastInst>(Exp)) {
		BitCastInst* bitcastInst = dyn_cast<BitCastInst>(Exp);
		Exp = bitcastInst->getOperand(0);
		LLVM_DEBUG(dbgs() << "	BitCast Instruction: " << *bitcastInst
				  << "\n";);
		LLVM_DEBUG(dbgs() << "	New RHS: " << *Exp << "\n";);
	}
	// handles pointer type
	if (Exp->getType()->getTypeID() == 15) {
		RHSisAddress = true;
	}
	if (Instruction* PreInst = dyn_cast<Instruction>(Exp)) {
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
			// check if instruction is of type x.f = ... or
			// x -> f = ...
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
					type = PreAllocaPreInstRHSPreGEPInst
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
	}
	if (!base) {
		// variable is probably a function parameter
		// or a global variable
		// Update functionArg
		base = dyn_cast<Instruction>(Exp);
		functionArg = Exp;
		type = Exp->getType();
	}
	if (!type) {
		Exp->getType();
	}
}

/* Constructor for RHSExpression
 * For an input Value* Exp it generates a RHSExpression object for it
 * It resets meta data and uses getMetaData to abstract meta data
 */
RHSExpression::RHSExpression(Value* Exp) {
	resetMetadata();
	LLVM_DEBUG(dbgs() << "	Initialize RHS with " << *Exp << "\n";);
	getMetaData(Exp);
}

/* getMetaData
 * It extracts metadata from Value* Exp using some fixed pattern in the IR
 */
void RHSExpression::getMetaData(Value* Exp) {
	// If the variable is a global variable use it
	if (handleGlobalVariable(Exp)) return;
	if (Exp && isa<BitCastInst>(Exp)) {
		BitCastInst* bitcastInst = dyn_cast<BitCastInst>(Exp);
		Exp = bitcastInst->getOperand(0);
		LLVM_DEBUG(dbgs() << "	BitCast Instruction: " << *bitcastInst
				  << "\n";);
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
		LLVM_DEBUG(dbgs() << "	Replaced constant " << *CI << "\n";);
		// replace calls with constant since this is a
		// context-insensitive analysis
		symbol = constant;
	} else if (AllocaInst* PreAllocaInst = dyn_cast<AllocaInst>(Exp)) {
		LLVM_DEBUG(dbgs() << "	Found alloca: " << *PreAllocaInst
				  << "\n";);
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
		LLVM_DEBUG(dbgs() << "	Found load : " << *Exp << "\n";);
		// if the previous instruction is of type load then
		// following cases are possible x = ... *x = ... x.f =
		// ... x -> f = ...
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
			// check if instruction is of type x.f = ... or
			// x -> f =
			// ...
			optional = PreGEPInst;
			Value* RHSPreGEPInst;
			for (auto& op : cast<User>(PreGEPInst)->operands()) {
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
				// if instruction is of type x -> f =
				// ...
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
	} else if (CallInst* CI = dyn_cast<CallInst>(Exp)) {
		LLVM_DEBUG(dbgs() << "	Replace call " << *CI << "with constant"
				  << "\n";);
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

/* UpdateInst takes a Store Instruction as an input
 * It abstract meta data of format LHS = RHS
 * which is object of class UpdateInst
 */
UpdateInst::UpdateInst(StoreInst* I) {
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

LLVMIRPlusPlusPass::LLVMIRPlusPlusPass() : ModulePass(ID) {}

bool LLVMIRPlusPlusPass::runOnModule(Module& M) {
	for (Function& F : M) {
		// Iterate over basicblocks
		for (BasicBlock& BB : F) {
			// Iterate over Instructions
			for (Instruction& I : BB) {
				LLVM_DEBUG(dbgs() << ">>>> " << I << "\n";);
				// For every store instruction
				if (StoreInst* StoreI =
					dyn_cast<StoreInst>(&I)) {
					LLVM_DEBUG(dbgs() << ">>>>>>>>	" << I
							  << "\n";);
					// Generate meta-data for store
					// instruction
					generateMetaData(StoreI);
				}
			}
		}
	}
	Module *Mod = &M;
	CFG* cfg = new CFG();
	cfg -> init(Mod);
	grcfg = &*cfg;
	return false;
}

/* generateMetaData
 * It force mimic the generation of metadata for any store instruction
 * It init an object of UpdateInst which in turn generate metadata for
 * LHS and RHS
 */
void LLVMIRPlusPlusPass::generateMetaData(StoreInst* StoreI) {
	UpdateInst* UpdateI = new UpdateInst(StoreI);
	IRPlusPlus[StoreI] = UpdateI;
	Expression* L = UpdateI->LHS;
	printExp(L);
	LLVM_DEBUG(dbgs() << " = ";);
	Expression* R = UpdateI->RHS;
	if (R->RHSisAddress && L->RHSisAddress && (R->type != L->type)) {
		LLVM_DEBUG(dbgs() << " & ";);
		R->symbol = address;
	}
	printExp(R);
	LLVM_DEBUG(dbgs() << "\n";);
}

void LLVMIRPlusPlusPass::printExp(Expression* L) {
	// prints the expression
	if (L->symbol == constant) {
		LLVM_DEBUG(dbgs() << "constant";);
		return;
	}
	if (L->type) {
		LLVM_DEBUG(dbgs() << " [ " << *(L->type) << " ] "
				  << " ";);
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

void Node::resetNode(){
	Inst = nullptr;
	LHS = nullptr;
	RHS = nullptr;
	abstractedInto = ir;
	loc = -1;
}

NodeList Node::getSucc(){
	// Returns the successors from the abstracted CFG
	// If the successor of any node is not abstracted 
	// it goes a level up until it finds a node which
	// is abstracted
	NodeList ans;
	NodeList WorkList;
	WorkList = getRealSucc();
	while(!WorkList.empty()){
		Node* temp = WorkList.back();
		WorkList.pop_back();
		if(temp -> abstractedInto != ir){
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

NodeList Node::getPred(){
	// Returns the predecessors from the abstracted CFG
	// If the predecessor of any node is not abstracted 
	// it goes a level down until it finds a node which
	// is abstracted
	NodeList ans;
	NodeList WorkList;
	WorkList = getRealPred();
	while(!WorkList.empty()){
		Node* temp = WorkList.back();
		WorkList.pop_back();
		if(temp -> abstractedInto != ir){
			ans.push_back(temp);
		} else {
			std::vector<Node*> p = temp -> getRealPred();
			for(Node* n : p){
				WorkList.push_back(n);
			}
		}
	}
	return ans;
}

NodeList Node::getRealSucc(){
	// returns both abstracted and unabstracted successor nodes
	return Succ;
}

NodeList Node::getRealPred(){
	// returns both abstracted and unabstracted predecessor nodes
	return Pred;
}

Node::Node(){
	resetNode();
}

Node::Node(Instruction *I){
	LLVM_DEBUG(dbgs() << " ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ \n";);
	resetNode();
	if(I){
		LLVM_DEBUG(dbgs() << "Working on instruction " << *I << " \n";);
		BasicBlock* parent = I -> getParent();
		LLVM_DEBUG(dbgs() << "BasicBlock is " << *parent << " \n";);
		Instruction* BBStartInst = &(parent -> front());
		LLVM_DEBUG(dbgs() << "BasicBlock starts with " << *BBStartInst << " \n";);
		Instruction* BBEndInst = &(parent -> back());
		LLVM_DEBUG(dbgs() << "BasicBlock ends with " << *BBEndInst << " \n";);
		Inst = I;
		// Stores are abstracted
		if(isa<StoreInst>(I)){
			abstractedInto = update;
		}
		// If abstracted then calculate LHS and RHS 
		if(abstractedInto == update){
			StoreInst* storeInst = dyn_cast<StoreInst>(I);
			if(storeInst){
				LHS = IRPlusPlus[storeInst] -> LHS;
				RHS = IRPlusPlus[storeInst] -> RHS;
			}
		}
		if(I == BBEndInst){
			// The instruction is the last instruction of the basic block
			// It's successor would be the first instruction of all it's
			// successor basic block
			LLVM_DEBUG(dbgs() << "Instruction is the end instruction \n";);
			for(BasicBlock *S : successors(parent)){
				Node* tempNode = new Node(&(S -> front()));
				(tempNode -> Pred).push_back(this);
				Succ.push_back(tempNode);
			}
		} else if(I == BBStartInst){
			// The instruction is the start instruction of the basic block
			// It's predecessors would be the last instructions of the predecessor 
			// basic block
			LLVM_DEBUG(dbgs() << "Instruction is the start instruction \n";);
			Instruction* SuccInstruction = nullptr;
			SuccInstruction = I -> getNextNonDebugInstruction();
			if(SuccInstruction && SuccInstruction -> getParent() == parent){
				Node* tempNode = new Node(SuccInstruction);
				(tempNode -> Pred).push_back(tempNode);
				Succ.push_back(tempNode);
			}
		} else {
			LLVM_DEBUG(dbgs() << "Instruction is in middle of BB \n";);
			Instruction* SuccInstruction = nullptr;
			SuccInstruction = I -> getNextNonDebugInstruction();
			if(SuccInstruction && SuccInstruction -> getParent() == parent){
				Node* tempNode = new Node(SuccInstruction);
				(tempNode -> Pred).push_back(this);
				Succ.push_back(tempNode);
			}
		}
	}
}

CFG::CFG(){
	StartNode = nullptr;
	EndNode = nullptr;
}

void CFG::init(Module* M){
	// Check if the cfg already exist
	if(StartNode){
		// The cgf is already inited
		return;
	}
	// Iterate over module
	for(Function& F : *M){
		// Iterate over functions 
		for(BasicBlock& BB : F){
			// Iterate over basicblocks
			for(Instruction& I : BB){
				// create a temp node for the first
				// instruction, this will be the unique
				// entry node
				Node* tempNode = new Node(&I);
				// If no entry node is present make this
				// an entry node
				if(!StartNode){
					// No entry node present
					StartNode = tempNode;
				}
				// There can be multiple leaves in the 
				// tree which are stored in EndInstList
				NodeList EndInstList;
				// Traverse over all the nodes until the
				// worklist is empty
				NodeList WorkList;
				WorkList.push_back(StartNode);
				while(! WorkList.empty()){
					// Take a node from worklist
					Node* temp = WorkList.back();
					// Remove it from worklist
					WorkList.pop_back();
					// If it is an leave node put it into 
					// EndInstList
					if((temp -> getRealSucc()).size() == 0){
						// temp is a leaf node
						EndInstList.push_back(temp);
					}
					// Insert successors of present node in
					// the worklist
					for(Node* s : temp -> getRealSucc()){
						WorkList.push_back(s);
					}
				}
				// endNode is the exit node
				// It is the pseudo node and is the only node with
				// Instruction field null
				Node* endNode = new Node;
				for(Node* end : WorkList){
					// Make edge between return nodes and end nodes
					// 	R1	R2	R3
					//	\\	||     //
					// 	 \\	||    //	
					// 	  \\    ||   //
					// 	      endNode
					(end -> getRealSucc()).push_back(endNode);
					(endNode -> getRealPred()).push_back(end);
				}
				break;
			}
		}
	}
}


InstMetaMap LLVMIRPlusPlusPass::getIRPlusPlus() { return IRPlusPlus; }
CFG* LLVMIRPlusPlusPass::getCFG(){ return grcfg; }

char LLVMIRPlusPlusPass::ID = 0;
static RegisterPass<LLVMIRPlusPlusPass> X(
    "llvmir++",					     // the option name
    "LLVM IR abstracted to C++ three address code",  // option description
    true,  // true as we don't modify the CFG
    true   // true if we're writing an analysis
);
