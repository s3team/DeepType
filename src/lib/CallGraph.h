#ifndef _CALL_GRAPH_H
#define _CALL_GRAPH_H

#include "Analyzer.h"
#include "Common.h"
#include <vector>

using namespace llvm;

class CallGraphPass : public IterativeModulePass {

	private:
		const DataLayout *DL;
		// char * or void *
		Type *Int8PtrTy;
		// long interger type
		Type *IntPtrTy;
		
		// ================Data Structures================
		// Multi-layer mapping
		static std::map<std::string, StrSet> FirstMap;
		static std::map<std::string, StrSet> SecondMap;
		static std::map<std::string, StrSet> ThirdMap;
		static std::map<std::string, StrSet> FourthMap;
		static std::map<std::string, StrSet> FifthMap;
		static std::map<std::string, StrSet> SixthMap;
		static std::map<std::string, StrSet> SeventhMap;
		
		// WMLTA Type-Func Map
		static DenseMap<size_t, FuncSet> WMLTATypeFuncMap;
		
		// Type-Func Map
		static DenseMap<size_t, FuncSet> MLTypeFuncMap;
		
		// A cache for quick lookup
		static DenseMap<size_t, FuncSet> TargetLookupMap;
		
		// Hash-Type Map
		static std::map<size_t, std::string> ReferMap;
		
		// StructID-StructName, help anonymous structs find names
		static std::map<std::string, StrSet> StructIDNameMap;
		
		// Extract complete multi-layer type for global variables
		static std::map<Value*, Value*> GVChildParentMap;
		static std::map<std::pair<Value*,Value*>, int> GVChildParentOffsetMap;
		static std::map<GlobalVariable*, FuncSet> GVFuncMap;
		static std::map<std::pair<GlobalVariable*,Function*>, std::string> GVFuncTypeMap;
		
		// Help check if an argument is an input
		static std::set<AllocaInst*> ArgAllocaSet;
		
		// Deal with compound instructions
		static std::map<Instruction*, std::map<unsigned, Instruction*>> InstHierarchy;
		
		// Type-Type Map, record friend types
		static std::map<std::string, StrSet> TypeRelationshipMap;
		
		// Organized Type-Type Map: a cache for quick search of friend types
		static std::map<std::string, StrSet> NewTypeRelationshipMap;
		
		// A cache for quick search of variant types
		//static std::map<std::string, StrSet> VariantTypeMap;
		
		// A cache for quick search of matched types
		static std::map<size_t, StrSet> MatchedTyMap;

		// A cache for quick search of a multi-layer type's friend types
		static std::map<std::string, StrSet> FriendTyMap;

		// Record derived classes for virtual functions
		static std::map<Value*, std::string> DerivedClassMap;
		
		// Record escaping types
		static std::set<std::string> EscapingSet;
		static std::set<size_t> UnsupportedSet;
		
		// For evaluation use
		static std::set<std::string> ManyTargetType;
		
		
		// ==================Functions====================
		void IterativeGlobalVariable(GlobalVariable *GVouter, GlobalVariable *GVinner, Value *v);
		void GVTypeFunctionRecord(GlobalVariable *GV, Function *F, Value *v, std::string FTyName);
		Value *NextLayerTypeExtraction(Value *v);
		bool isClassType(std::string SrcTyName);
		std::string StripClassType(std::string classTy);		
		std::string GenClassTyName(std::string SrcTyName);	
		void GlobalVariableAnalysis(GlobalVariable *GV, Constant *Ini);
		void UnfoldCompoundInst(Instruction *I);
		void CompoundInstAnalysis(Instruction *I);
		void MemCpyInstAnalysis(Instruction *MCI);
		void StoreInstAnalysis(StoreInst *SI);
		void SelectInstAnalysis(SelectInst *SI);
		std::string CastInstAnalysis(CastInst *CastI);
		std::string FunctionCastAnalysis(Function *F, CastInst *CastI);
		void CallInstAnalysis(CallInst *CallI);
		std::string GEPInstAnalysis(GetElementPtrInst *GEPI);
		std::string SingleType2String (Type *Ty);
		bool IsCompositeType(Type *Ty);
		bool IsCompositeTypeStr(std::string TyStr);
		bool IsCompoundInst(Instruction *I);
		bool IsGeneralPointer(Type *Ty);
		bool IsUnsupportedType(Type *Ty);
		bool IsUnsupportedTypeStr(std::string TyStr);
		bool IsEscapingType(std::string TyName);
		std::string GetStructIdentity(StructType* STy);
		bool HasSubString(std::string str, std::string substr);				
		std::string GenerateMLTypeName(Value *VO, std::string MLTypeName);
		std::string StructNameTrim(std::string sName);
		std::size_t FindEndOfStruct(std::string structstr);
		std::string FirstLayerTrim(std::string fName);
		void UpdateRelationshipMap(std::string CType, std::string FType);
		void UpdateMLTypeFuncMap(std::string type, Function* F);
		void CalculateVariantTypes(std::string TyStr);
		
		void FindCalleesWithSMLTA(CallInst *CI);
		void ExhaustiveSearch4FriendTypes(std::string Search4Type);
		FuncSet FSMerge(FuncSet FS1, FuncSet FS2);
		FuncSet FSIntersect(FuncSet FS1, FuncSet FS2);
		StrSet TypeSetMerge(StrSet S1, StrSet S2);
		StrPairSet FindSubTypes(std::string MLTypeName);
		void PrintResults(CallInst *CI, FuncSet FS, std::string MLTypeName);
		list<std::string> MLTypeName2List(std::string MLTypeName);
		void PrintMaps();
		StrSet UpgradeTypeRelationshipMap(std::string key, bool first);
		bool LayerMatch(std::string s1, std::string s2);
		bool MLTypeMatch(list<std::string> CList, list<std::string> GList);
		bool IsValidType(std::string ft);
		StrSet AddFuzzyTypeAndCopySet(StrSet CumuSet);
		StrSet LookupTypeRecordMap(std::string t, int map);
		StrSet UpdateCumuTySet(StrSet CumuTySet, int map, std::string LayerType);
		StrSet CoverAll(StrSet CumuTySet, int map);
		void printSet(StrSet Set);

	public:
		CallGraphPass(GlobalContext *Ctx_)
			: IterativeModulePass(Ctx_, "CallGraph") { }
		
		virtual bool CollectInformation(Module *M);
		//virtual bool doFinalization(llvm::Module *);
		virtual bool IdentifyTargets(llvm::Module *M);

};

#endif
