//===-- Analyzer.cc - the kernel-analysis framework-------------===//
// 
// It constructs a global call-graph based on multi-layer type
// analysis.
//
//===-----------------------------------------------------------===//
 
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
//#include "llvm/Bitcode/BitcodeReader.h"
//#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
//#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/Support/FileSystem.h"
//#include "llvm/IRReader/IRReader.h"
//#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/Path.h"

#include <memory>
#include <vector>
#include <sstream>
#include <sys/resource.h>
#include <chrono>
#include <iomanip>
#include <iostream>

#include "Analyzer.h"
#include "CallGraph.h"

using namespace llvm;
using namespace std;


GlobalContext GlobalCtx;

void IterativeModulePass::run(Module *M) {
	
	OP << "SMLTA Stage 1: Collecting Information...";
  	CollectInformation(M);
  	OP << "Done.\n";
  	OP << "SMLTA Stage 2: Identifying Targets...";
  	IdentifyTargets(M);
  	OP << "Done.\n\n";
}

void PrintResults(GlobalContext *GCtx) {

	int TotalTargets = 0;
	for (auto IC : GCtx->IndirectCallInsts) {
		TotalTargets += GCtx->Callees[IC].size();
	}
	unsigned WithTargetIndirectCalls = GCtx->IndirectCallInsts.size() - GCtx->NoTargetCalls;
	float AveIndirectTargets = 0;
	if (WithTargetIndirectCalls > 0)
		AveIndirectTargets = (float)GCtx->NumIndirectCallTargets/(float)WithTargetIndirectCalls;

	OP<<"############## DeepType Result Statistics ##############\n";
	OP<<"# Number of indirect calls: \t\t\t"<<GCtx->IndirectCallInsts.size()<<"\n";
	OP<<"# Number of indirect-call targets: \t\t"<<GCtx->NumIndirectCallTargets<<"\n";
	OP<<"# Number of address-taken functions: \t\t"<<GCtx->AddressTakenFuncs.size()<<"\n";
	//OP<<"# Number of more than 3 layer call site type: \t\t"<<GCtx->NumThreeLayerType<<"\n";	
	OP<<"# Number of 0-target i-calls: \t\t\t"<<GCtx->NoTargetCalls<<"\n";
	std::cout << "# Ave. Number of indirect-call targets: \t" 
				<< std::fixed << std::setprecision(2) << AveIndirectTargets << "\n";
	//OP<<"# Number of [1,2)-target i-calls: \t\t"<<GCtx->ZerotTargetCalls<<"\n";
	//OP<<"# Number of [2,4)-target i-calls: \t\t"<<GCtx->OnetTargetCalls<<"\n";
	//OP<<"# Number of [4,8)-target i-calls: \t\t"<<GCtx->TwotTargetCalls<<"\n";
	//OP<<"# Number of [8,16)-target i-calls: \t\t"<<GCtx->ThreetTargetCalls<<"\n";
	//OP<<"# Number of [16,32)-target i-calls: \t\t"<<GCtx->FourtTargetCalls<<"\n";
	//OP<<"# Number of [32,64)-target i-calls: \t\t"<<GCtx->FivetTargetCalls<<"\n";
	//OP<<"# Number of [64,128)-target i-calls: \t\t"<<GCtx->SixtTargetCalls<<"\n";
	//OP<<"# Number of [128,256)-target i-calls: \t\t"<<GCtx->SeventTargetCalls<<"\n";
	//OP<<"# Number of [256,...)-target i-calls: \t\t"<<GCtx->EighttTargetCalls<<"\n";
}

std::unordered_map<CallInst*, std::unordered_set<Function*>> SMLTAnalysis(Module *M) {
	
	//GlobalCtx.Modules.push_back(std::make_pair(M, M->getModuleIdentifier()));
    	//GlobalCtx.ModuleMaps[M] = M->getModuleIdentifier();

    	//IterativeModulePass Pass(&GlobalCtx, "SMLT");
    	//Pass.run(M);
    	
    	// Build global callgraph.
	CallGraphPass CGPass(&GlobalCtx);
	CGPass.run(M);

   	PrintResults(&GlobalCtx);

    	return GlobalCtx.SMLTAResultMap;
}

