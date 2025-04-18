#ifndef _ANALYZER_GLOBAL_H
#define _ANALYZER_GLOBAL_H

#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include "llvm/Support/CommandLine.h"
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "Common.h"



// 
// typedefs
//
typedef std::vector< std::pair<llvm::Module*, llvm::StringRef> > ModuleList;
// Mapping module to its file name.
typedef std::unordered_map<llvm::Module*, llvm::StringRef> ModuleNameMap;
// The set of all functions.
typedef llvm::SmallPtrSet<llvm::Function*, 8> FuncSet;
// The set of strings.
typedef std::set<std::string> StrSet;
// The pair of an array and its size
typedef std::pair<std::string*, int> ArrayPair;
// The set of string pairs.
typedef std::set<std::pair<std::string, std::string>> StrPairSet;
// Mapping from function name to function.
typedef std::unordered_map<std::string, llvm::Function*> NameFuncMap;
typedef llvm::SmallPtrSet<llvm::CallInst*, 8> CallInstSet;
typedef DenseMap<Function*, CallInstSet> CallerMap;
typedef DenseMap<CallInst *, FuncSet> CalleeMap;
typedef std::pair<std::string, AllocaInst*> TyRepkeypair;


struct GlobalContext {

	GlobalContext() {
		// Initialize statistucs.
		NumFunctions = 0;
		NumFirstLayerTypeCalls = 0;
		NumSecondLayerTypeCalls = 0;
		NumIndirectCallTargets = 0;
		NoTargetCalls = 0; 		// Number of indirect calls which have no target
		ZerotTargetCalls = 0;		// [1,2) 2^0 ~ 2^1
		OnetTargetCalls = 0;		// [2,4) 2^1 ~
		TwotTargetCalls = 0;		// [4,8) 2^2 ~
		ThreetTargetCalls = 0;		// [8,16) 2^3 ~
		FourtTargetCalls = 0;		// [16,32) 2^4 ~
		FivetTargetCalls = 0;		// [32,64) 2^5 ~
		SixtTargetCalls = 0;		// [64,128) 2^6 ~
		SeventTargetCalls = 0;		// [128,256) 2^7 ~
		EighttTargetCalls = 0;		// [256,...) 2^8 ~
	}

	// Statistics 
	unsigned NumFunctions;
	unsigned NumFirstLayerTypeCalls;
	unsigned NumSecondLayerTypeCalls;
	unsigned NumIndirectCallTargets;
	unsigned NoTargetCalls;
	unsigned ZerotTargetCalls;
	unsigned OnetTargetCalls;
	unsigned TwotTargetCalls;
	unsigned ThreetTargetCalls;
	unsigned FourtTargetCalls;
	unsigned FivetTargetCalls;
	unsigned SixtTargetCalls;
	unsigned SeventTargetCalls;
	unsigned EighttTargetCalls;
	unsigned NumThreeLayerType;
	

	// Map global function name to function.
	NameFuncMap GlobalFuncs;

	// Functions whose addresses are taken.
	FuncSet AddressTakenFuncs;

	// Map a callsite to all potential callee functions.
	CalleeMap Callees;

	// Map a function to all potential caller instructions.
	CallerMap Callers;

	// Unified functions -- no redundant inline functions
	DenseMap<size_t, Function *>UnifiedFuncMap;
	set<Function *>UnifiedFuncSet;

	// Map function signature to functions
	DenseMap<size_t, FuncSet>sigFuncsMap;

	// Indirect call instructions.
	std::vector<CallInst *>IndirectCallInsts;

	// Indirect call-target mappings
	std::unordered_map<CallInst*, FuncSet> SMLTAResultMap;

	// Modules.
	ModuleList Modules;
	ModuleNameMap ModuleMaps;
	std::set<std::string> InvolvedModules;

};

class IterativeModulePass {
protected:
	GlobalContext *Ctx;
	const char * ID;
public:
	IterativeModulePass(GlobalContext *Ctx_, const char *ID_)
		: Ctx(Ctx_), ID(ID_) { }

	// api
	void SMLTAnalysis(const std::vector<std::string> &InputFilenames);

	// Run on each module before iterative pass.
	virtual bool CollectInformation(Module *M)
		{ return true; }

	// Run on each module after iterative pass.
	//virtual bool doFinalization(llvm::Module *M)
	//	{ return true; }

	// Iterative pass.
	virtual bool IdentifyTargets(llvm::Module *M)
		{ return false; }

	virtual void run(ModuleList &modules);
};

#endif
