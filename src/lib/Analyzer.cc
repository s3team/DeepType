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
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
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
//#include "Config.h"

using namespace llvm;
using namespace std;

auto mid = std::chrono::system_clock::now();;

// Command line parameters.
cl::list<std::string> InputFilenames(
    cl::Positional, cl::OneOrMore, cl::desc("<input bitcode files>"));

cl::opt<unsigned> VerboseLevel(
    "verbose-level", cl::desc("Print information at which verbose level"),
    cl::init(0));

cl::opt<bool> MLTA(
    "mlta", 
	cl::desc("Multi-layer type analysis for refining indirect-call \
		targets"), 
	cl::NotHidden, cl::init(false));

GlobalContext GlobalCtx;


void IterativeModulePass::run(ModuleList &modules) {

  ModuleList::iterator i, e;
  OP << "[" << ID << "] Initializing " << modules.size() << " modules " << "\n";
  bool again = true;
  while (again) {
    again = false;
    for (i = modules.begin(), e = modules.end(); i != e; ++i) {
      again |= CollectInformation(i->first);
      OP << "\n";
    }
  }
  OP << "\n";

  mid = std::chrono::system_clock::now();

  unsigned iter = 0, changed = 1;
  while (changed) {
    ++iter;
    changed = 0;
    unsigned counter_modules = 0;
    unsigned total_modules = modules.size();
    for (i = modules.begin(), e = modules.end(); i != e; ++i) {
      OP << "[" << ID << " / " << iter << "] ";
      OP << "[" << ++counter_modules << " / " << total_modules << "] ";
      OP << "[" << i->second << "]\n";

      bool ret = IdentifyTargets(i->first);
      if (ret) {
        ++changed;
        OP << "\t [CHANGED]\n";
      } else
        OP << "\n";
    }
    OP << "[" << ID << "] Updated in " << changed << " modules.\n";
  }

  //OP << "[" << ID << "] Postprocessing ...\n";
/* 
  again = true;
  while (again) {
    again = false;
    for (i = modules.begin(), e = modules.end(); i != e; ++i) {
      // TODO: Dump the results.
      again |= doFinalization(i->first);
    }
  }
*/

  OP << "[" << ID << "] Done!\n\n";
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

std::unordered_map<CallInst*, FuncSet> SMLTAnalysis(const std::vector<std::string> &InputFilenames) {
	
	for (unsigned i = 0; i < InputFilenames.size(); ++i) {

		LLVMContext *LLVMCtx = new LLVMContext();
		SMDiagnostic Err;
		std::unique_ptr<Module> M = parseIRFile(InputFilenames[i], Err, *LLVMCtx);

		if (M == NULL) {
			OP << argv[0] << ": error loading file '"
				<< InputFilenames[i] << "'\n";
			continue;
		}

		Module *Module = M.release();
		StringRef MName = StringRef(strdup(InputFilenames[i].data()));
		GlobalCtx.Modules.push_back(std::make_pair(Module, MName));
		GlobalCtx.ModuleMaps[Module] = InputFilenames[i];
	}

	//
	// Main workflow
	//
	
	// Build global callgraph.
	CallGraphPass CGPass(&GlobalCtx);
	CGPass.run(GlobalCtx.Modules);

	return GCtx->SMLTAResultMap;
}

int main(int argc, char **argv) {
	auto start = std::chrono::system_clock::now();
	
	// Print a stack trace if we signal out.
	sys::PrintStackTraceOnErrorSignal(argv[0]);
	PrettyStackTraceProgram X(argc, argv);

	llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.

	cl::ParseCommandLineOptions(argc, argv, "global analysis\n");

	// Loading modules
	OP << "Total " << InputFilenames.size() << " file(s)\n";

	std::vector<std::string> inputs(InputFilenames.begin(), InputFilenames.end());

	SMLTAnalysis(inputs);

	// Print final results
	std::unordered_map<CallInst*, FuncSet> resultMap = PrintResults(&GlobalCtx);
	
	auto end = std::chrono::system_clock::now();
	std::cout << "Stage 1 " << std::chrono::duration_cast<std::chrono::milliseconds>(mid-start).count() << " ms" << std::endl;
	std::cout << "Stage 2 " << std::chrono::duration_cast<std::chrono::milliseconds>(end-mid).count() << " ms" << std::endl;
	std::cout << "total " << std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << " ms" << std::endl;
	return 0;
}

