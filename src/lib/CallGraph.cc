//===-- CallGraph.cc - Build global call-graph------------------===//
// 
// This pass builds a global call-graph. The targets of an indirect
// call are identified based on type-analysis, i.e., matching the
// number and type of function parameters.
//
//===-----------------------------------------------------------===//

#include <llvm/Pass.h>
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Debug.h"
#include <llvm/IR/Module.h>
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"  
#include "llvm/IR/InstrTypes.h" 
#include "llvm/IR/BasicBlock.h" 
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include <map> 
#include <set>
#include <algorithm>
#include <vector> 
#include <utility>
#include <iostream>
#include <fstream>
#include <bits/stdc++.h>
#include "llvm/IR/CFG.h" 
#include "llvm/Transforms/Utils/BasicBlockUtils.h" 
#include "llvm/IR/IRBuilder.h"
#include "CallGraph.h"
#include "Common.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/CallGraph.h"  
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Operator.h"

//#define DTweak
//#define DTnoSH
//#define DTnocache

using namespace llvm;

std::map<std::string, StrSet> CallGraphPass::FirstMap;
std::map<std::string, StrSet> CallGraphPass::SecondMap;
std::map<std::string, StrSet> CallGraphPass::ThirdMap;
std::map<std::string, StrSet> CallGraphPass::FourthMap;
std::map<std::string, StrSet> CallGraphPass::FifthMap;
std::map<std::string, StrSet> CallGraphPass::SixthMap;
std::map<std::string, StrSet> CallGraphPass::SeventhMap;
DenseMap<size_t, FuncSet> CallGraphPass::WMLTATypeFuncMap;
DenseMap<size_t, FuncSet> CallGraphPass::MLTypeFuncMap;
DenseMap<size_t, FuncSet> CallGraphPass::TargetLookupMap;
std::map<size_t, std::string> CallGraphPass::ReferMap;
std::map<std::string, std::set<std::string>> CallGraphPass::StructIDNameMap;
std::map<Value*, Value*> CallGraphPass::GVChildParentMap;
std::map<std::pair<Value*,Value*>, int> CallGraphPass::GVChildParentOffsetMap;
std::map<GlobalVariable*, FuncSet> CallGraphPass::GVFuncMap;
std::map<std::pair<GlobalVariable*,Function*>, std::string> CallGraphPass::GVFuncTypeMap;
std::set<AllocaInst*> CallGraphPass::ArgAllocaSet;
std::map<Instruction*, std::map<unsigned, Instruction*>> CallGraphPass::InstHierarchy;
std::map<std::string, std::set<std::string>> CallGraphPass::TypeRelationshipMap;
std::map<std::string, StrSet> CallGraphPass::NewTypeRelationshipMap;
std::map<std::string, StrSet> CallGraphPass::FriendTyMap;
//std::map<std::string, StrSet> CallGraphPass::VariantTypeMap;
std::map<size_t, StrSet> CallGraphPass::MatchedTyMap;
std::map<Value*, std::string> CallGraphPass::DerivedClassMap;
std::set<std::string> CallGraphPass::EscapingSet;
std::set<size_t> CallGraphPass::UnsupportedSet;
std::set<std::string> TypeNameSet;
std::set<size_t>LayerNumSet;
int LayerNumArray[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
std::set<std::string> CallGraphPass::ManyTargetType;


// Global variables
std::string TyName;
bool AIFlag = false;
AllocaInst *RecordAI;
int CSIdx = 0;

bool CallGraphPass::IsCompositeType(Type *Ty) {
	while (PointerType *PTy = dyn_cast<PointerType>(Ty)) {
		Ty = PTy->getPointerElementType();
	}
	if (Ty->isStructTy() || Ty->isArrayTy() || Ty->isVectorTy())
		return true;
	else 
		return false;
}

bool CallGraphPass::IsGeneralPointer(Type *Ty) {
	while (PointerType *PTy = dyn_cast<PointerType>(Ty)) {
		Ty = PTy->getPointerElementType();
	}
	return Ty->isIntegerTy();
}

bool CallGraphPass::IsUnsupportedType(Type *Ty) {
	if (UnsupportedSet.find(typeHash(Ty)) != UnsupportedSet.end())
		return true;
	else
		return false;
}

bool CallGraphPass::IsUnsupportedTypeStr(std::string TyStr) {
	if (TyStr.substr(0,1) == "i" && !HasSubString(TyStr, "(") && !HasSubString(TyStr, ")")) {
		std::string s = TyStr.substr(1, (TyStr.length()-1));
		while (s.substr((s.length()-1), 1) == "*") {
			s = s.substr(0, (s.length()-1));
		}
		for (char const &c : s) {
        	if (std::isdigit(c) == 0) {
        		return false;
        	}
    	}
    	return true;
	}
	else {
		return false;
	}
}

bool CallGraphPass::IsCompositeTypeStr(std::string TyStr) {
	if (TyStr.substr(0,6) == "struct" || 
		TyStr.substr(0,5) == "array" || 
		TyStr.substr(0,6) == "vector" ||
		TyStr.substr(0,5) == "union") {
		return true;		
	}
	else {
		return false;
	}
}

bool CallGraphPass::IsEscapingType(std::string TypeName) {
	if (EscapingSet.find(TypeName) != EscapingSet.end())
		return true;
	else
		return false;
}

bool CallGraphPass::IsCompoundInst(Instruction *I) {
	unsigned operandNum = I->getNumOperands();
	for (unsigned index = 0; index < operandNum; index++) {
		Value *operand = I->getOperand(index);
		if (operand->getType()->isPointerTy() && isa<ConstantExpr>(operand)) {
			return true;
		}
	}
	return false;
}

bool CallGraphPass::HasSubString(std::string str, std::string substr) {
	std::string::size_type idx;
	idx = str.find(substr);
	if (idx == std::string::npos) {
		return false;
	}
	else {
		return true;
	}
}

std::string CallGraphPass::GetStructIdentity(StructType* STy) {
	std::string STyID = "";
	for (Type* Ty : STy->elements()) {
		STyID += SingleType2String(Ty);
	}
	return STyID;
}

// Given a struct name, remove suffix at the end if exists
std::string CallGraphPass::StructNameTrim(std::string sName) {
	std::size_t idx = sName.find_last_of('.');
	std::size_t check = sName.find_first_of('.');	
	
	// Literal structs are named as struct.num, do not remove .num
	// Identified structs may be named as struct.name.suffix, remove .suffix	
	if (idx != check) {
		std::string sNameTail = sName.substr(idx+1);
		std::string sNameHead = sName.substr(0, idx);		 		
		for (char c: sNameTail) {
			if (isdigit(c)) {
				continue;
			}
			else {
				return sName;
			}
		}
		return sNameHead;
	}
	else {
		return sName;
	}
}

std::size_t CallGraphPass::FindEndOfStruct(std::string structstr) {
	std::size_t end = 0;
	for (char c: structstr) {		
		if (c=='*' || c==',' || c=='(' || c==')') {
			break;
		}
		end++;
	}

	return end;
}

// Given MLType name, remove suffix at the end of a struct in first layer if exists
std::string CallGraphPass::FirstLayerTrim(std::string fName) {	
	std::size_t idx, end;
	std::string clearName = "";
	std::string sName;
	std::string head, body, tail;

	while ((idx = fName.find("%struct")) != std::string::npos) {
		// Find the index of the first character after struct name
		end = FindEndOfStruct(fName.substr(idx));

		// Divide fName into 3 parts: before the struct, the struct, after the struct
		head = fName.substr(0,idx);
		body = fName.substr(idx,end);
		tail = fName.substr(idx+end);

		sName = StructNameTrim(body);
		clearName = clearName + head + sName;
		fName = tail; 
	}
	
	clearName += fName;
	return clearName;
}

std::string CallGraphPass::GenerateMLTypeName(Instruction *loc, Value *VO, std::string MLTypeName) {	
	VO = NextLayerTypeExtraction(VO);
	while (VO != NULL) {
		if (ConstantExpr *CE = dyn_cast<ConstantExpr>(VO)) {
			Instruction *Inst = CE->getAsInstruction();
			Inst->insertBefore(loc);
			if (GetElementPtrInst *GEPInst = dyn_cast<GetElementPtrInst>(Inst)) {
				MLTypeName = GEPInstAnalysis(GEPInst);
			}
			break;	
		}
		if (TyName != "") {
			MLTypeName += "|" + TyName;
			TyName = "";
		}
		VO = NextLayerTypeExtraction(VO);				
	}
	
	return MLTypeName;
}


// Transform a layer's type to string
std::string CallGraphPass::SingleType2String (Type *Ty) {
	std::string TypeName;
	
	if (IsCompositeType(Ty)) {
		while (PointerType *PTy = dyn_cast<PointerType>(Ty)) {
			Ty = PTy->getPointerElementType();
		}	
		if (StructType* STy = dyn_cast<StructType>(Ty)) { 			
			TypeName = Ty->getStructName().str();
 			
 			if (TypeName.length() == 0) {
 				std::string STyID = GetStructIdentity(STy);
 				if (StructIDNameMap.find(STyID) != StructIDNameMap.end()) {
 					TypeName = "NAMESET";
 					std::set<std::string> TyNameSet = StructIDNameMap[STyID];
 					TypeNameSet.clear();
 					TypeNameSet.insert(TyNameSet.begin(), TyNameSet.end());	
 				}
 				else {
 					TypeName = "";	
					TypeName = "struct.anon";
 				}	 			
	 		}
 			 		
	 		TypeName = StructNameTrim(TypeName);		 		
	 	}
	 	else if (Ty->isArrayTy()) {	 		
			TypeName = "array";
	 		std::string array_str;
			llvm::raw_string_ostream rso(array_str);
			Ty->print(rso);
	 		TypeName += FirstLayerTrim(rso.str());
	 	}
	 	else if (Ty->isVectorTy()) {	 		
			TypeName = "vector";
	 		std::string vector_str;
			llvm::raw_string_ostream rso(vector_str);
			Ty->print(rso);
	 		TypeName += FirstLayerTrim(rso.str());
	 	}
	}
	else {
		std::string type_str;
		llvm::raw_string_ostream rso(type_str);
		Ty->print(rso);
 		TypeName = rso.str();
 		TypeName = FirstLayerTrim(TypeName);		
	}
	return TypeName;
}


Value *CallGraphPass::NextLayerTypeExtraction(Value *v) {	
	// Case 1: GetElementPtrInst
	if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(v)) {
		// If a GEPI only has the first index
		// Then, the source type of the GEPI is not an outer layer type
		if (GEP->getNumIndices() == 1) {
			TyName = "";
			return GEP->getPointerOperand();
		}
		
		Type *Ty = GEP->getPointerOperandType();
		if (IsCompositeType(Ty)) {			
			unsigned opNum = GEP->getNumOperands();
			if (ConstantInt* CInt = dyn_cast<ConstantInt>(GEP->getOperand(opNum-1))) {
				int offsetNum = CInt->getSExtValue();
				std::string offsetstr = std::to_string(offsetNum);
				TyName = SingleType2String(Ty) + "#" + offsetstr;
			}
			else {
				TyName = SingleType2String(Ty) + "#?";
			}
			return GEP->getPointerOperand();
		}		
		else {
			TyName = "";
			return NULL;
		}

/*		
		Type *Ty = GEP->getPointerOperandType();
		if (IsCompositeType(Ty)) {				
			TyName = SingleType2String(Ty);
			return GEP->getPointerOperand();
		}		
		else {
			TyName = "";
			return NULL;
		}
*/
	}
	
	// Case 2: LoadInst
	else if (LoadInst *LI = dyn_cast<LoadInst>(v)) {
		TyName = "";
		return NextLayerTypeExtraction(LI->getOperand(0));
	}
	
	// Case 3: ConstantExpr
	else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(v)) {
		TyName = "";
		return CE;
	}
	
	// Case 4: AllocaInst
	else if (AllocaInst *AI = dyn_cast<AllocaInst>(v)) {
		//if (AllocaInst *AI = dyn_cast<AllocaInst>(UI)) {
		TyName = "";
		AIFlag = true;
		RecordAI = AI;
		return NULL;
	}
	
	// Case 5: Bitcast
	else if (CastInst *CI = dyn_cast<CastInst>(v)) {
		Type *SrcTy = CI->getSrcTy();
		std::string STyName = SingleType2String(SrcTy);

#ifdef DTnoSH
		TyName = "";
		return NULL;
#endif
		
		// bitcast {}* to some_type: Special handling 4
		if (STyName.length() == 0) {
			TyName = "";
			return NextLayerTypeExtraction(CI->getOperand(0));
		}
		else {
			TyName = "";
			return NULL;
		}				
	}
	
	else {
		TyName = "";
		return NULL;
	}
	
	return v;
}

/*
void CallGraphPass::CalculateVariantTypes(std::string TyStr) {
	std::string TyStr_copy = TyStr;
	std::replace(TyStr_copy.begin(), TyStr_copy.end(), '#', '|');
	
	list<std::string> TyList = MLTypeName2List(TyStr_copy);
	StrSet CumuSet;
	StrSet VarSet;
	StrSet NewCumuSet;
	
	std::string LayerTy = TyList.front();
	TyList.pop_front();
	CumuSet.insert(LayerTy);
	if (TyList.empty()) {
		VarSet.insert(LayerTy);
	}
	
	std::string CumuTy;
	
	while (!TyList.empty()) {
		LayerTy = TyList.front();
		TyList.pop_front();
		
		bool digit = true;
		for (auto c: LayerTy) {
			if (!isdigit(c)) {
				digit = false;
				break;
			}
		}
		
		for (StrSet::iterator it=CumuSet.begin(); it!=CumuSet.end(); it++) {
			std::string base = *it;
			if (LayerTy == "&") {
				VarSet.insert(base);
			}
			CumuTy = base + "|" + LayerTy;
			NewCumuSet.insert(CumuTy);
			if (TyList.empty()) {
				VarSet.insert(CumuTy);
			}
			if (LayerTy.substr(0,6) == "struct") {
				CumuTy = base + "|struct.anon";
				NewCumuSet.insert(CumuTy);
			}
			if (digit == true) {
				CumuTy = base + "|?";
			}
			else {
				CumuTy = base + "|&";
			}
			VarSet.insert(CumuTy);
		}
		CumuSet = NewCumuSet;
		NewCumuSet.clear();
	}
	
	VariantTypeMap[TyStr] = VarSet;
	return;
}*/

void CallGraphPass::UpdateRelationshipMap(std::string CType, std::string FType) {
	if (!IsUnsupportedTypeStr(CType) && !IsUnsupportedTypeStr(FType)) {
		if (IsCompositeTypeStr(CType) && IsCompositeTypeStr(FType)) {
			TypeRelationshipMap[CType].insert(FType);
		}
		if (!IsCompositeTypeStr(CType) && !IsCompositeTypeStr(FType)) {
			TypeRelationshipMap[CType].insert(FType);
		}
	}
	
	return;
}

void CallGraphPass::UpdateMLTypeFuncMap(std::string type, Function* F) {
	MLTypeFuncMap[stringHash(type)].insert(F);
	ReferMap[stringHash(type)] = type;
	
	std::string CumuType;
	list<std::string> typeList;
	typeList = MLTypeName2List(type);
	
	std::string FirstLayer = typeList.front();
	typeList.pop_front();
	
	if (!typeList.empty()) {	// >=2 layers		
		std::string SecondLayer = typeList.front();
		FirstMap[FirstLayer].insert(SecondLayer);
		//errs() << "Cumu Layer: " << FirstLayer << " 2nd Layer: " << SecondLayer << "\n";
		typeList.pop_front();
		
		if (!typeList.empty()) {	// >=3 layers
			CumuType = FirstLayer + "|" + SecondLayer;
			std::string ThirdLayer = typeList.front();
			SecondMap[CumuType].insert(ThirdLayer);
			//errs() << "Cumu Layer: " << CumuType << " 3rd Layer: " << ThirdLayer << "\n";
			typeList.pop_front();
		
			if (!typeList.empty()) {	// >=4 layers
				CumuType = CumuType + "|" + ThirdLayer;
				std::string FourthLayer = typeList.front();
				ThirdMap[CumuType].insert(FourthLayer);
				//errs() << "Cumu Layer: " << CumuType << " 4th Layer: " << FourthLayer << "\n";
				typeList.pop_front();
				
				if (!typeList.empty()) {	// >=5 layers
					CumuType = CumuType + "|" + FourthLayer;
					std::string FifthLayer = typeList.front();
					FourthMap[CumuType].insert(FifthLayer);
					//errs() << "Cumu Layer: " << CumuType << " 5th Layer: " << FifthLayer << "\n";
					typeList.pop_front();
					
					if (!typeList.empty()) {	// >=6 layers
						CumuType = CumuType + "|" + FifthLayer;
						std::string SixthLayer = typeList.front();
						FifthMap[CumuType].insert(SixthLayer);
						//errs() << "Cumu Layer: " << CumuType << " 6th Layer: " << SixthLayer << "\n";
						typeList.pop_front();
						
						if (!typeList.empty()) {	// >=7 layers
							CumuType = CumuType + "|" + SixthLayer;
							std::string SeventhLayer = typeList.front();
							SixthMap[CumuType].insert(SeventhLayer);
							//errs() << "Cumu Layer: " << CumuType << " 7th Layer: " << SeventhLayer << "\n";
							typeList.pop_front();
							
							if (!typeList.empty()) {	// Use & to repressent afterwards types
								CumuType = CumuType + "|" + SeventhLayer;
								SeventhMap[CumuType].insert("&");
								//errs() << "Cumu Layer: " << CumuType << " 8th Layer: " << "&" << "\n";
							}
						}
					}
				}
			}
		}
	}	
	
#ifdef DTweak
	typeList.clear();
	typeList = MLTypeName2List(type);
	std::string LayTy;

	while (!typeList.empty()) {
		LayTy = typeList.front();
		WMLTATypeFuncMap[stringHash(LayTy)].insert(F);

		// Confine F to matched types
		int pos = LayTy.find_last_of("#");
		std::string LayTyName;
		std::string LayTyIdx;
		std::string MatchedType;		
				
		if (pos > 0) {	// Has index, not first layer type
			LayTyName = LayTy.substr(0,pos);
			LayTyIdx = LayTy.substr(pos); 
			if (LayTy.substr(0,6) == "struct") {
				// Match with struct.anon#idx
				MatchedType = "struct.anon" + LayTyIdx;
				WMLTATypeFuncMap[stringHash(MatchedType)].insert(F);		
				// Match with struct.anon#?
				MatchedType = "struct.anon#?";
				WMLTATypeFuncMap[stringHash(MatchedType)].insert(F);
			}
			
			// Store in LayTyName#all to match with LayTyName#?
			MatchedType = LayTyName + "#?";
			WMLTATypeFuncMap[stringHash(MatchedType)].insert(F);								
		}
		
		typeList.pop_front();
	}
#endif	
	
	return;
}

bool CallGraphPass::isClassType(std::string SrcTyName) {
	if (SrcTyName.find("(%class.") != string::npos) {
		return true;	
	}
	return false;
}

std::string CallGraphPass::StripClassType(std::string classTy) {
	if (classTy[0] == '%') {
		classTy = classTy.substr(1, (classTy.size()-1));	
	}
	while (classTy.back() == '*') {
		classTy = classTy.substr(0, (classTy.size()-1));
	}
	return classTy;
}

std::string CallGraphPass::GenClassTyName(std::string SrcTyName) {
	std::string substr = "%class.";	
	int start = SrcTyName.find(substr);
	int end = SrcTyName.find(",", start);

	std::string classTy = SrcTyName.substr(start, (end-start));
	classTy = StripClassType(classTy);	
	std::string funcTy1 = SrcTyName.substr(0, start);
	std::string funcTy2 = SrcTyName.substr(end+2, (SrcTyName.size()-end-2));
	std::string ClassTyName = funcTy1 + funcTy2 + "|" + classTy;

	return ClassTyName;
}


// ==================== Stage 1 ====================
void CallGraphPass::GVTypeFunctionRecord(GlobalVariable *GV, Function *F, Value *v, std::string FTyName) {
	//errs() << "GVTypeFunctionRecord" << "\n";
	int offset;
	Value *PV;
	Type *vTy;
	std::string MLTypeName;
	std::string MLTypeName_backup;
	std::string STypeName;
	MLTypeName = FTyName;
	
	while (GVChildParentMap.find(v) != GVChildParentMap.end()) {
		PV = GVChildParentMap[v];
		offset = GVChildParentOffsetMap[std::make_pair(v, PV)];
		v = PV;
		vTy = v->getType();
		STypeName = SingleType2String(vTy);
		if (STypeName != "NAMESET") {
			MLTypeName += "|" + STypeName + "#" + std::to_string(offset);
			
			// Record this type in MLTypeFuncMap
			UpdateMLTypeFuncMap(MLTypeName, F);	
			//errs() << "1 " << "Type: " << MLTypeName << " Target: " << F->getName().str() << "\n";
			//errs() << "GV: " << GV->getName().str() << "\n";
			//ReferMap[stringHash(MLTypeName)] = MLTypeName;
	
			// Reserve info. for interative GV		
			GVFuncMap[GV].insert(F);
			GVFuncTypeMap[make_pair(GV, F)] = MLTypeName;
		}
		else {
			MLTypeName_backup = MLTypeName;
			for (std::string TyName: TypeNameSet) {
				MLTypeName = MLTypeName_backup;
				MLTypeName += "|" + TyName + "#" + std::to_string(offset);
				
				// Record this type in MLTypeFuncMap	
				UpdateMLTypeFuncMap(MLTypeName, F);
				//errs() << "2 " << "Type: " << MLTypeName << " Target: " << F->getName().str() << "\n";
				//errs() << "GV: " << GV->getName().str() << "\n";
				//ReferMap[stringHash(MLTypeName)] = MLTypeName;
	
				// Reserve info. for interative GV		
				GVFuncMap[GV].insert(F);
				GVFuncTypeMap[make_pair(GV, F)] = MLTypeName;
			}
		}
	}
	
	return;
}


void CallGraphPass::IterativeGlobalVariable(GlobalVariable *GVouter, GlobalVariable *GVinner, Value *v) {
	
	int offset;
	Value* PV;
	std::string MLTypeName;	
	std::string MLTypeName_backup;
	std::string STypeName;
	
	// Share GVinner's function with GVouter
	FuncSet FS = GVFuncMap[GVinner];
	if (FS.empty()) {
		return;
	}
	
	for (auto F:FS) {										
		MLTypeName = GVFuncTypeMap[make_pair(GVinner, F)];	
		//errs() << "MLTypeName from GVinners: " << MLTypeName << "\n";
		Type *vTy;
		Value *key = v;
		while (GVChildParentMap.find(key) != GVChildParentMap.end()) {
			PV = GVChildParentMap[key];
			offset = GVChildParentOffsetMap[std::make_pair(key, PV)];
			key = PV;
			vTy = key->getType();
			STypeName = SingleType2String(vTy);
			if (STypeName != "NAMESET") {
				MLTypeName += "|" + STypeName + "#" + std::to_string(offset);
				
				// Record this type in MLTypeFuncMap
				UpdateMLTypeFuncMap(MLTypeName, F);	
		
				// Reserve info. for interative GV		
				GVFuncMap[GVouter].insert(F);
				GVFuncTypeMap[make_pair(GVouter, F)] = MLTypeName;
			}
			else {
				MLTypeName_backup = MLTypeName;
				for (std::string TyName: TypeNameSet) {
					MLTypeName = MLTypeName_backup;
					MLTypeName += "|" + TyName + "#" + std::to_string(offset);
					
					// Record this type in MLTypeFuncMap	
					UpdateMLTypeFuncMap(MLTypeName, F);
		
					// Reserve info. for interative GV		
					GVFuncMap[GVouter].insert(F);
					GVFuncTypeMap[make_pair(GVouter, F)] = MLTypeName;
				}
			}
		}		
	}
	
	return;
}


void CallGraphPass::GlobalVariableAnalysis(GlobalVariable *GV, Constant *Ini){
	//errs() << "GV: " << GV->getName().str() << "\n";
	GVChildParentMap.clear();
	GVChildParentOffsetMap.clear();
	// Check if the Initializer is ConstantAggregate
	if (isa<ConstantAggregate>(Ini)){
		list<User *> IniList;
		IniList.push_back(Ini);
		
		while (!IniList.empty()) {
			User *U = IniList.front();			
			IniList.pop_front();
			Type *UTy = U->getType();			
			Value *V = U;
			std::string FTyName;

			if (IsCompositeType(UTy) && !isa<PointerType>(UTy)) {
				int offset = 0;
				for (auto oi = U->op_begin(), oe = U->op_end(); oi != oe; ++oi) {
					Value *ov = *oi;
					//errs() << "element: " << *ov << "\n";
					GVChildParentMap[ov] = V;
					GVChildParentOffsetMap[std::make_pair(ov, V)] = offset;
					Type *OTy = ov->getType();

					if (IsCompositeType(OTy) && !isa<PointerType>(OTy)) {
						//errs() << "OTy is composite type" << "\n";
						User *ou = dyn_cast<User>(ov);
						IniList.push_back(ou);
					}
					else if (Function *F = dyn_cast<Function>(ov)) {
						//errs() << "ov is function" << "\n";
						// Record F as a valid target of GV's multi-layer type
						FTyName = SingleType2String(ov->getType());
						GVTypeFunctionRecord(GV, F, ov, FTyName);
					}		
					else if (GlobalVariable *GVinner = dyn_cast<GlobalVariable>(ov)) {
						//errs() << "ov is gv" << "\n";
						IterativeGlobalVariable(GV, GVinner, ov);
					}
					else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(ov)) {
						Instruction *Inst = CE->getAsInstruction();
						if (!GV->getParent()->empty()) {
        						Function &FirstFunc = GV->getParent()->getFunctionList().front();
        						BasicBlock &Entry = FirstFunc.getEntryBlock();
        						Inst->insertBefore(&*Entry.getFirstInsertionPt());
    						} else {
        						Inst->deleteValue();  // prevent memory leak
        						return;
    						}

						Value *operand = Inst->getOperand(0);
						if (GlobalVariable *GVinner = dyn_cast<GlobalVariable>(operand)) {
							IterativeGlobalVariable(GV, GVinner, ov);
						}
						else if (CastInst *CI = dyn_cast<CastInst>(Inst)) {
							Value *op = CI->getOperand(0);
							//errs() << "op(0): " << *op << "\n";
							Function *F = dyn_cast<Function>(ov->stripPointerCasts());
							if (F != NULL) {
								//errs() << "bitcast function" << "\n";
								Type *DestTy = CI->getDestTy();
								Type *SrcTy = CI->getSrcTy();								
								std::string SrcTyName = SingleType2String(SrcTy);
								if (isClassType(SrcTyName)) {
									std::string ClassTyName = GenClassTyName(SrcTyName);									
									UpdateMLTypeFuncMap(ClassTyName, F);
								}
								else {
									FTyName = SingleType2String(DestTy);
									GVTypeFunctionRecord(GV, F, ov, FTyName);
								}
							}
						}
					}
					else if (IsGeneralPointer(OTy) || OTy->isIntegerTy()) {
						// Do nothing
					}
					else if (isa<ConstantPointerNull>(ov)) {
						// Do nothing
					}
					else {
						//errs() << "1 Unexpected global variable initialization: " << *ov << "\n";
					}		
					offset++;
				}
			}
			else if (Function *F = dyn_cast<Function>(V)) {
				FTyName = SingleType2String(V->getType());
				GVTypeFunctionRecord(GV, F, V, FTyName);
			}
			else {
				//errs() << "2 Unexpected Global variable Initialization: " << *V << "\n";
			}
		}
	}
	else if (Function *Func = dyn_cast<Function>(Ini)) { // Initializer is a function
		Type *GTy = Ini->getType();
		std::string GTyName = SingleType2String(GTy);
		
		UpdateMLTypeFuncMap(GTyName, Func);
	}
	return;
}


// Unfold compound instruction and build an inst hierarchy
// Embeded inst is a child of the compound inst
void CallGraphPass::UnfoldCompoundInst(Instruction *I) {
	// Record inst hierarchy info. in InstHierarchy map:
	// Multi-level map: compoud Inst - index of operand - embedded inst
	unsigned operandNum = I->getNumOperands();
	for (unsigned index = 0; index < operandNum; index++) {
		Value *operand = I->getOperand(index);
		if (operand->getType()->isPointerTy() && isa<ConstantExpr>(operand)) {
			// Unfold compound inst
			// Transform the ConstantExpr into an instruction
			ConstantExpr *CE = dyn_cast<ConstantExpr>(operand);
			Instruction * newInst = CE->getAsInstruction();
			newInst->insertBefore(I);
			InstHierarchy[I][index] = newInst;
			UnfoldCompoundInst(newInst);
		}
	}
		
	return;
}


void CallGraphPass::MemCpyInstAnalysis(Instruction *I){
	
	if (MemCpyInst *MCI = dyn_cast<MemCpyInst>(I)) {	
		std::string DTypeName;
		std::string STypeName;
		Value *dest = MCI->getRawDest();
		Value *src = MCI->getRawSource();
		CastInst *destCI;
		CastInst *srcCI;
		
		// Extract SrcType and DestType from MemCpyInst
		if (isa<CastInst>(dest)) {
			destCI = dyn_cast<CastInst>(dest);
		}	
		else if (ConstantExpr *destCE = dyn_cast<ConstantExpr>(dest)) {
			Instruction *destInst = destCE->getAsInstruction();
			destInst->insertBefore(I);
			if (isa<CastInst>(destInst)) {
				destCI = dyn_cast<CastInst>(destInst);
			}
		}
		else {return;}
		
		if (isa<CastInst>(src)) {
			srcCI = dyn_cast<CastInst>(src);
		}
		else if (ConstantExpr *srcCE = dyn_cast<ConstantExpr>(src)) {
			Instruction *srcInst = srcCE->getAsInstruction();
			srcInst->insertBefore(I);
			if (isa<CastInst>(srcInst)) {
				srcCI = dyn_cast<CastInst>(srcInst);
			}
		}
		else {return;}
	
		Type *destTy = destCI->getSrcTy();
		Type *srcTy = srcCI->getSrcTy();
		
		// Trace back to get src/dest type name
		if (isa<PointerType>(srcTy) && isa<PointerType>(destTy)
			&& !IsGeneralPointer(srcTy) && !IsGeneralPointer(destTy)) {
			
			// Get first-layer type
			DTypeName = SingleType2String(destTy);
			STypeName = SingleType2String(srcTy);
			
			// Trace back to get next layer(s)
			DTypeName = GenerateMLTypeName(destCI, destCI->getOperand(0), DTypeName);
			STypeName = GenerateMLTypeName(srcCI, srcCI->getOperand(0), STypeName);
			
			// STypeName shares targets with DTypeName
			UpdateRelationshipMap(DTypeName, STypeName);
		}
	}
		
	return;
}

void CallGraphPass::StoreInstAnalysis(StoreInst *SI) {
	
	Value *VO = SI->getValueOperand();
	Value *PO = SI->getPointerOperand();
	Type *VTy = VO->getType();
	Type *PTy = PO->getType();
	std::string VTypeName;
	std::string PTypeName;
	Instruction *EI; // Embedded inst
	
	if (PointerType *PPTy = dyn_cast<PointerType>(PTy)) {
		PTy = PPTy->getPointerElementType();
	}
	
	// Collect unsupported types
	if (IsGeneralPointer(PTy) || isa<IntegerType>(PTy)) {
		UnsupportedSet.insert(typeHash(PTy));
	}
	if (IsGeneralPointer(VTy) || isa<IntegerType>(VTy)) {
		UnsupportedSet.insert(typeHash(VTy));
	}

	// Analyze PO to get PO's multi-layer type
	// PO is an embedded inst
	EI = InstHierarchy[SI][1];
	if (EI) {
		if (CastInst *ECI = dyn_cast<CastInst>(EI)) {
			PTypeName = CastInstAnalysis(ECI);
		}
		else if (GetElementPtrInst *EGEPI = dyn_cast<GetElementPtrInst>(EI)) {
			PTypeName = GEPInstAnalysis(EGEPI);
		}
	}
	// PO is not an embedded inst
	else {	
		PTypeName = SingleType2String(PTy);
		PTypeName = GenerateMLTypeName(SI, PO, PTypeName);		
	}
	
	// Analyze VO and record useful info.
	// VO is an embedded inst
	EI = InstHierarchy[SI][0];
	if (EI) {
		if (CastInst *ECI = dyn_cast<CastInst>(EI)) {
			VTypeName = CastInstAnalysis(ECI);
		}
		else if (GetElementPtrInst *EGEPI = dyn_cast<GetElementPtrInst>(EI)) {
			VTypeName = GEPInstAnalysis(EGEPI);
		}
		UpdateRelationshipMap(PTypeName, VTypeName);
	}
	// VO is not an embedded inst
	else {
		// VO is a function
		if (Function *F = dyn_cast<Function>(VO)) {	    	
			if (PTypeName.length() != 0) {
				UpdateMLTypeFuncMap(PTypeName, F);
			} 			
		}
		
		// VO is a pointer
		else if (isa<PointerType>(VTy)) {
			VTypeName = SingleType2String(VTy);
			VTypeName = GenerateMLTypeName(SI, VO, VTypeName);
			if (VTypeName != PTypeName) {
				UpdateRelationshipMap(PTypeName, VTypeName);
			}	
		}
		
		// VO is a SelectInst
		else if (SelectInst *SeI = dyn_cast<SelectInst>(VO)) {		
			Value *TV = SeI->getTrueValue();
			Value *FV = SeI->getFalseValue();

			// Conservatively record functions in T/F branches as targets
			if (Function *F = dyn_cast<Function>(TV)) {
				if (PTypeName.length() != 0) {
					UpdateMLTypeFuncMap(PTypeName, F);
					//ReferMap[stringHash(PTypeName)] = PTypeName;
				}
			}
			if (Function *F = dyn_cast<Function>(FV)) {			
				if (PTypeName.length() != 0) {
					UpdateMLTypeFuncMap(PTypeName, F);
					//ReferMap[stringHash(PTypeName)] = PTypeName;
				}
			}
		}
		
		// VO is a CastInst
		else if (CastInst *CastI = dyn_cast<CastInst>(VO)) { 
			Type *CastSrc = CastI->getSrcTy();
			VTypeName = SingleType2String(CastSrc);
			VTypeName = GenerateMLTypeName(CastI, CastI->getOperand(0), VTypeName);
			if (VTypeName != PTypeName) {
				UpdateRelationshipMap(PTypeName, VTypeName);
			}			
		}
		
		// VO is a CallInst
		else if (CallInst *CaI = dyn_cast<CallInst>(VO)) {
			Function *CalledFunc = CaI->getCalledFunction();
			if (CalledFunc) {		
				if (PTypeName.length() != 0) {
					UpdateMLTypeFuncMap(PTypeName, CalledFunc);
					//ReferMap[stringHash(PTypeName)] = PTypeName;
				}
			}
		}
		
		// VO is a global variable
		else if (GlobalVariable *GV = dyn_cast<GlobalVariable>(VO)) {
			VTypeName = SingleType2String(VTy);
			UpdateRelationshipMap(PTypeName, VTypeName);
		}		
	}
			
	return;
}

void CallGraphPass::SelectInstAnalysis(SelectInst *SI) {		
	Value *TV = SI->getTrueValue();
	Value *FV = SI->getFalseValue();

	// Conservatively record functions in T/F branches as targets
	if (Function *F = dyn_cast<Function>(TV)) {
		Type *FTy = F->getType();
		std::string FTyName = SingleType2String(FTy);
		FTyName += "|&";
		UpdateMLTypeFuncMap(FTyName, F);
	}
	if (Function *F = dyn_cast<Function>(FV)) {			
		Type *FTy = F->getType();
		std::string FTyName = SingleType2String(FTy);
		FTyName += "|&";
		UpdateMLTypeFuncMap(FTyName, F);
	}
	return;	
}

// Analyze cast instruction and return dest type's multi-layer type name
std::string CallGraphPass::CastInstAnalysis(CastInst *CI) {	
	//errs() << "Cast Inst: " << *CI << "\n";
	
	std::string STypeName;
	std::string DTypeName;
	Type *SrcTy = CI->getSrcTy();
	Type *DestTy = CI->getDestTy();
	
	// Collect unsupported types
	if (IsGeneralPointer(SrcTy) || isa<IntegerType>(SrcTy)) {
		UnsupportedSet.insert(typeHash(SrcTy));
	}
	if (IsGeneralPointer(DestTy) || isa<IntegerType>(DestTy)) {
		UnsupportedSet.insert(typeHash(DestTy));
	}
		
	// Escape type check:
	// If a composite type is cast from/to unsupported type, then it is escape type
	if (IsUnsupportedType(SrcTy) && IsCompositeType(DestTy)) {
		EscapingSet.insert(SingleType2String(DestTy));
	}
	if (IsUnsupportedType(DestTy) && IsCompositeType(SrcTy)) {
		EscapingSet.insert(SingleType2String(SrcTy));
	}
	
	//  SrcTy is an embedded inst
	if (Instruction *EI = InstHierarchy[CI][0]) {
		if (GetElementPtrInst *EGEPI = dyn_cast<GetElementPtrInst>(EI)) {
			STypeName = GEPInstAnalysis(EGEPI);
		}
	}
	// SrcTy is not an embedded inst
	else {
		STypeName = SingleType2String(SrcTy);
		STypeName = GenerateMLTypeName(CI, CI->getOperand(0), STypeName);
	}
	
	//  DestTy is an embedded inst
	if (Instruction *EI = InstHierarchy[CI][1]) {
		if (GetElementPtrInst *EGEPI = dyn_cast<GetElementPtrInst>(EI)) {
			DTypeName = GEPInstAnalysis(EGEPI);
		}
	}
	// DestTy is not an embedded inst
	else {
		DTypeName = SingleType2String(DestTy);
		unsigned operandNum = CI->getNumOperands();
		if (operandNum > 1) {
			DTypeName = GenerateMLTypeName(CI, CI->getOperand(1), DTypeName);
		}	
	}	
	
	// Record type relationship
	if (DTypeName.length() == 0 && STypeName.length() !=0) {
		UpdateRelationshipMap(DTypeName, STypeName);
	}
	else if (STypeName.length() == 0 && DTypeName.length() !=0) {
		UpdateRelationshipMap(STypeName, DTypeName);
	}
	else if (STypeName.length() == 0 && DTypeName.length() ==0) {
		// Do NOT record in the map
	}
	else {
		UpdateRelationshipMap(STypeName, DTypeName);
		UpdateRelationshipMap(DTypeName, STypeName);
	}
	
	return DTypeName;
}


std::string CallGraphPass::FunctionCastAnalysis(Function *F, CastInst *CastI) {
	Type *SrcTy = CastI->getSrcTy();
	Type *DestTy = CastI->getDestTy();
	std::string STypeName = SingleType2String(SrcTy);
	std::string DTypeName = SingleType2String(DestTy);
	
	// Update maps
	UpdateMLTypeFuncMap(STypeName, F);	
	UpdateMLTypeFuncMap(DTypeName, F);	
	UpdateRelationshipMap(STypeName, DTypeName);
	UpdateRelationshipMap(DTypeName, STypeName);
	
	return DTypeName;
}


void CallGraphPass::CallInstAnalysis(CallInst *CallI) {
	Value *V = CallI->getCalledOperand();
	std::string FTyName;
	std::string MLTypeName;
	Type *FTy;

	if (ConstantExpr *CE = dyn_cast<ConstantExpr>(V)){
		// e.g., call i8* bitcast (i8*(...) @func to i8*(...))(...)
		Instruction *Inst = CE->getAsInstruction();
		Inst->insertBefore(CallI);
		if (CastInst *CastI = dyn_cast<CastInst>(Inst)) {
			Function *F = dyn_cast<Function>(V->stripPointerCasts());
			if (F != NULL) {
				FunctionCastAnalysis(F, CastI);
			}
		}
		
	} 
	
	// Initialization through the arg(s) of call inst
	// e.g., call i32* @func (i8*(i32)* @target)
	// Record target and i8*(i32)* in MLTypeFuncMap.
	// Trace back to see if i8*(i32)* has outer-layer type.
	// If it has, record in TypeRelationshipMap.
	for (auto ai = CallI->arg_begin(), ae = CallI->arg_end(); ai != ae; ++ai) {
		Value *arg = *ai;
		Type *argType = arg->getType();
		// Trace back to get arg's multi-layer type
		MLTypeName = SingleType2String(argType);
		MLTypeName = GenerateMLTypeName(CallI, arg, MLTypeName);
		
		if (Function *Func = dyn_cast<Function>(arg)) {
			// arg's first layer type
			FTyName = SingleType2String(Func->getType());
			
			if (FTyName != MLTypeName) {
				UpdateMLTypeFuncMap(MLTypeName, Func);
			}
			else {
				MLTypeName = FTyName + "|&"; // conservative
				UpdateMLTypeFuncMap(MLTypeName, Func);
			}
		}
	}
	
	if (DbgValueInst *dbgvI = dyn_cast<DbgValueInst>(CallI)) {	
		Value *v = dbgvI->getValue();
		if (v) {
			if (Function *dbgF = dyn_cast<Function>(v)) {
				FTyName = SingleType2String(dbgF->getType());
				FTyName += "|&";
				UpdateMLTypeFuncMap(FTyName, dbgF);
			}
		}
	}
	
	return;
}

// Extract multi-layer type from GEPInst
// Return structName#offset
std::string CallGraphPass::GEPInstAnalysis(GetElementPtrInst *GEPI) {
	// If the indexes end with 0, there are two equivalent types in this GEPInst	
	std::string MLTypeName;
	std::string EqualTypeName;  
	std::string LayerTyName;
	list<std::string> TyNameList;
	list<std::string> TyNameList_backup;	
    Type *LayerTy = NULL;
    unsigned opNum = GEPI->getNumOperands();
    bool equal = false;
    
    uint64_t Idx;
    int offset = 0;
    int offsetNum;
    Type *SrcTy = GEPI->getSourceElementType();	
    for (auto oi = GEPI->idx_begin(), oe = GEPI->idx_end(); oi != oe; ++oi) {
    	// Get Layer Type each operand points to
    	offset++;
    	SmallVector<Value *, 8> Ops(GEPI->idx_begin(), GEPI->idx_begin() + offset);
		LayerTy = GetElementPtrInst::getIndexedType(SrcTy, Ops);
		LayerTyName = SingleType2String(LayerTy);
    	
    	// If an operand other than the first operand (operand(0)) is 0,
    	// there exists equivalent type
    	if (offset > 1 && GEPI->getOperand(offset) == 0) {
			equal = true;
    	}
    	
    	// Add offset at the end of LayerTyName
    	if (offset < (opNum-1)) {
			if (ConstantInt* CInt = dyn_cast<ConstantInt>(GEPI->getOperand(offset+1))) {
				offsetNum = CInt->getSExtValue();
				LayerTyName += "#" + std::to_string(offsetNum);
			}
			else {
				LayerTyName += "#?";
			}
		}			
		TyNameList.push_back(LayerTyName);
    }
    TyNameList_backup = TyNameList;

	// Get the multi-layer type name in GEPInst
	TyNameList_backup = TyNameList;
	if (!TyNameList.empty()) {
		MLTypeName = TyNameList.back();
		TyNameList.pop_back();
		while (!TyNameList.empty()){
			MLTypeName += "|" + TyNameList.back();
			TyNameList.pop_back();			
		}
	}
	else {
		MLTypeName = SingleType2String(SrcTy);
	}

	// Record equivalent types in TypeRelationshipMap
	if (equal == true) {		
		TyNameList_backup.pop_back();
		if (!TyNameList_backup.empty()) {
			EqualTypeName = TyNameList_backup.back();
			TyNameList_backup.pop_back();
			while (!TyNameList_backup.empty()) {
				EqualTypeName += "|" + TyNameList_backup.back();
				TyNameList_backup.pop_back();
			}

			if (MLTypeName != EqualTypeName) {
				UpdateRelationshipMap(MLTypeName, EqualTypeName);
				UpdateRelationshipMap(EqualTypeName, MLTypeName);
			}
		}			
	}	

	return MLTypeName;
}


bool CallGraphPass::CollectInformation(Module *M) {
	errs() << "Collecting information..." << "\n";

#ifdef DTnoSH
	goto POS2;
#endif
	
	// Record named structures in StructNameMap for future use: Special handling 2
	for (StructType* STy: M->getIdentifiedStructTypes()) {
		if (STy->hasName() && !STy->isOpaque()) {
			std::string STyID = GetStructIdentity(STy);
			std::string STyName = STy->getName().str();
			STyName = StructNameTrim(STyName);
			StructIDNameMap[STyID].insert(STyName);
		}
	}

	POS2:
	// Deal with global variables
	for (Module::global_iterator gi = M->global_begin(); gi != M->global_end(); ++gi) {		
		GlobalVariable* GV = &*gi;		
		if (!GV->hasInitializer()) {
			continue;
		}		
		Constant *Ini = GV->getInitializer();
		GlobalVariableAnalysis(GV, Ini);
	}
	
	for (Module::global_iterator gi = M->global_begin(); gi != M->global_end(); ++gi) {		
		GlobalVariable* GV = &*gi;		
		if (!GV->hasInitializer()) {
			continue;
		}		
		Constant *Ini = GV->getInitializer();
		GlobalVariableAnalysis(GV, Ini);
	}

	// Deal with instructions
	for (Function &F : *M) {

#ifdef DTnoSH
		goto POS3;
#endif
			
		// Skip dead functions: Special handling 3
		if (F.use_empty() && (F.getName().str() != "main")) {			
			continue;
		}

		POS3:	
		// Record arguments in ArgSet, the type of arg is an incomplete type
		std::set<Value *> ArgSet;	
		for (Function::arg_iterator ai = F.arg_begin(), ae = F.arg_end(); ai != ae; ++ai) {
			Value *arg = ai;
			ArgSet.insert(arg);
		}

		if (F.isDeclaration())
			continue;
			
		// Collect address-taken functions.
		if (F.hasAddressTaken()) {
			Ctx->AddressTakenFuncs.insert(&F);
		}

		for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
			Instruction *I = &*i;

#ifdef DTnoSH
			goto POS1;
#endif

			// Unfold and analyze compound inst: Special handling 1
			if (IsCompoundInst(I)) {
				InstHierarchy.clear();
				UnfoldCompoundInst(I);
			}
			
			POS1:
			// MemCpyInst
			if (isa<MemCpyInst>(I)) {
				MemCpyInstAnalysis(I);
			}	
		
			// StoreInst
			if (StoreInst *SI = dyn_cast<StoreInst>(I)) {				
				StoreInstAnalysis(SI);
				
				// Deal with incomplete type
				// store %0, %2
				// If %0 is an arg, %2 is an AllocaInst, record %2 in ArgAllocaSet
				// If a CallInst refers to AllocaInst %2, its type is incomplete
				Value *PO = SI->getPointerOperand();
				Value *VO = SI->getValueOperand();
				if (ArgSet.find(VO) != ArgSet.end() && isa<AllocaInst>(PO)) {
					AllocaInst *AI = dyn_cast<AllocaInst>(PO);
					ArgAllocaSet.insert(AI);					
				}
			}
			
			if (SelectInst *SeI = dyn_cast<SelectInst>(I)) {
				SelectInstAnalysis(SeI);
			}
	
			// CastInst
			if (CastInst *CastI = dyn_cast<CastInst>(I)) {
				Type *SrcTy = CastI->getSrcTy();
				Type *DstTy = CastI->getDestTy();
				CastInstAnalysis(CastI);
			}			
	
			// GetElementPtrInst
			if (GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(I)) {
				GEPInstAnalysis(GEPI);
			}
			
			// CallInst
			if (CallInst *CallI = dyn_cast<CallInst>(I)) {
				CallInstAnalysis(CallI);
			}
		}
	}

	return false;
}

// ==================== Stage 2 ====================

// Add all functions in FS2 to FS1
FuncSet CallGraphPass::FSMerge(FuncSet FS1, FuncSet FS2) {
	for (auto F: FS2) {
		FS1.insert(F);
	}
	return FS1;
}

// Return the intersection of FS1 and FS2
FuncSet CallGraphPass::FSIntersect(FuncSet FS1, FuncSet FS2) {
	FuncSet FS;
	for (auto F: FS1) {
		if (FS2.find(F) != FS2.end()) {
			FS.insert(F);
		}
	}
	return FS;
}

StrSet CallGraphPass::TypeSetMerge(StrSet S1, StrSet S2) {
	for (auto str: S2) {
		S1.insert(str);
	}
	return S1;
}

StrPairSet CallGraphPass::FindSubTypes(std::string MLTypeName) {
	StrPairSet SubTypeSet;
	
	// First pair: "" & MLTypeName
	SubTypeSet.insert(std::make_pair("",MLTypeName));
	
	// Other pairs: divide from "|"'s position
	int idx = 0;
	std::string head;
	std::string tail;
    while((idx = MLTypeName.find("|", idx)) != string::npos) {
        head = MLTypeName.substr(0, idx+1);
        tail = MLTypeName.substr(idx+1, (MLTypeName.size()-idx-1));
        SubTypeSet.insert(std::make_pair(head, tail));
        idx++;
    }
	
	return SubTypeSet;
}

void CallGraphPass::PrintResults(CallInst *CI, FuncSet FS, std::string MLTypeName) {
	
	// record in result map
	Ctx->SMLTAResultMap[CI] = FS;

	// Print Call site index
	CSIdx++;
	errs() << CSIdx << " ";
	errs() << "Call Site ";
	CI -> getDebugLoc().print(errs());
	errs() << "\n";
	errs() << "Call site type: " << MLTypeName << "\n";
	
	if (FS.empty()){
		errs() << "No target." << "\n";
	}
	else {
		vector<std::string> FuncNameVec;
		while(!FS.empty()){
			llvm::Function *CurFunc = *FS.begin();
			FuncNameVec.push_back(CurFunc->getName().str());
			FS.erase(CurFunc);
		}
		std::sort(FuncNameVec.begin(), FuncNameVec.end());
		for (std::string s:FuncNameVec){
			errs() << s << "\n";
		}
	}
		
	return;
}

list<std::string> CallGraphPass::MLTypeName2List(std::string MLTypeName) {
	
	// Get number of layers
	int LayerNum = 1;
	int i = 0;
	while ((i = MLTypeName.find("|", i)) != string::npos) {
        LayerNum++;
        i++;
    }
    
    std::string LayerName;
    list<std::string> LayerList;
    i = 0;
    int idx = 0;
    int j = 0;
	while ((i = MLTypeName.find("|", i)) != string::npos) {      
        LayerName = MLTypeName.substr(j, i-j);
        LayerList.push_back(LayerName);
        idx++;
        j = i+1;
        i++;       
    }
    LayerName = MLTypeName.substr(j, (MLTypeName.length()-j+1));
    LayerList.push_back(LayerName);
    
    return LayerList;
}


bool CallGraphPass::LayerMatch(std::string s1, std::string s2) {
	// s1 from call site's type
	// s2 from candidate's type (candidates are in MLTypeFuncMap)
	
	if (s1 == s2) {
		return true;
	}
	else if (s1.substr(0,1) == "&" || s2.substr(0,1) == "&") {
		return true;
	}
	else if (s1.substr(0,11) == "struct.anon") { // anonymous struct
		if (s2.substr(0,6) != "struct") {return false;}
		int pos1 = s1.find_last_of("#");
		int pos2 = s2.find_last_of("#");
		std::string s1Index = s1.substr(pos1);
		std::string s2Index = s2.substr(pos2);
		if (s1Index == s2Index) {	// index is the same
			return true;	
		}
		else {return false;}
	}
	else if (s2.substr(0,11) == "struct.anon") {
		if (s1.substr(0,6) != "struct") {return false;}
		int pos1 = s1.find_last_of("#");
		int pos2 = s2.find_last_of("#");
		std::string s1Index = s1.substr(pos1);
		std::string s2Index = s2.substr(pos2);
		if (s1Index == s2Index) {	// index is the same
			return true;	
		}
		else {return false;}
	}
	else if (s1.substr(s1.length()-1,1) == "?" || s2.substr(s2.length()-1,1) == "?" ){ // Unknown index
		int pos1 = s1.find_last_of("#");
		int pos2 = s2.find_last_of("#");
		std::string s1Name = s1.substr(0,pos1);
		std::string s2Name = s2.substr(0,pos2);
		if (s1Name == s2Name) {
			return true;
		}
		else {return false;}
	}
	else {return false;}
}


bool CallGraphPass::IsValidType(std::string ft) {
	list<std::string> ftList = MLTypeName2List(ft);

	// Ignore types with more than 7 layers
	if (ftList.size() > 7) {
		return false;
	}
	
	// First layer must be function pointer type
	std::string first = ftList.front();
	if (first.substr(first.length()-2, 2) != ")*") {
		return false;
	}
	
	// Other layers must be composite type or fuzzy type if exist
	// The adjacent two layers should not be identical
	ftList.pop_front();	
	std::string other, afterPop;
	std::size_t id1, id2;
	while (!ftList.empty()) {
		other = ftList.front();
		if (other == "&") {
			ftList.pop_front();
			if (ftList.empty()) {
				return true;		
			}
			else {
				return false;
			}
		}
		else if (other.substr(0, 6) == "struct" || 
				 	other.substr(0, 5) == "array" || 
					other.substr(0, 6) == "vector") {
			ftList.pop_front();
			if (!ftList.empty()) {
				afterPop= ftList.front();
				id1 = other.find("#");
				id2 = afterPop.find("#");
				if (id1 != std::string::npos && id2 !=std::string::npos && 
						other.substr(0,id1) == afterPop.substr(0,id2)) {					
					return false;
				}
			}		
		}
		else {
			return false;		
		}
	}		
	return true;
}

StrSet CallGraphPass::AddFuzzyTypeAndCopySet(StrSet CumuSet) {
	StrSet MatchedSet;
	std::string FuzzyType;
	for (std::string t: CumuSet) {
		if (t.substr(t.length()-1,1) == "&") {
			MatchedSet.insert(t);
		}
		else {
			FuzzyType = t + "|&";
			MatchedSet.insert(t);
			MatchedSet.insert(FuzzyType);
		}
	}

	return MatchedSet;
}

StrSet CallGraphPass::LookupTypeRecordMap(std::string t, int map) {
	StrSet Stmp;

	if (map == 1) {
		Stmp = FirstMap[t];
	}
	else if (map == 2) {
		Stmp = SecondMap[t];
	}
	else if (map == 3) {
		Stmp = ThirdMap[t];
	}
	else if (map == 4) {
		Stmp = FourthMap[t];
	}
	else if (map == 5) {
		Stmp = FifthMap[t];
	}
	else if (map == 6) {
		Stmp = SixthMap[t];
	}
	else if (map == 7) {
		Stmp = SeventhMap[t];
	}
	
	return Stmp;
}

StrSet CallGraphPass::UpdateCumuTySet(StrSet CumuTySet, int map, std::string LayerType) {
	StrSet NewSet;
	StrSet LayerTySet;
	std::string CumuType;
	
	for (std::string t: CumuTySet) {
		if (t.substr(t.length()-1,1) == "&") {
			NewSet.insert(t);
		}
		LayerTySet = LookupTypeRecordMap(t, map);
		for (std::string e: LayerTySet) {
			if (LayerMatch(e, LayerType)) {
				CumuType = t + "|" + e;
				NewSet.insert(CumuType);
			}
		}
	}

	return NewSet;
}

StrSet CallGraphPass::CoverAll(StrSet CumuTySet, int map) {
	StrSet MatchedTySet;
	StrSet LayerTySet;
	StrSet NewCumuTySet;
	std::string CumuType;
	
	while (map <= 7) {	
		for (std::string t: CumuTySet) {
			LayerTySet = LookupTypeRecordMap(t, map);
			for (std::string l: LayerTySet) {
				CumuType = t + "|" + l;
				NewCumuTySet.insert(CumuType);
				MatchedTySet.insert(CumuType);
			}
		}
		if (NewCumuTySet.empty()) {
			break;
		}
		CumuTySet = NewCumuTySet;
		NewCumuTySet.clear();
		map++;
	}
		
	return MatchedTySet;
}

void CallGraphPass::printSet(StrSet Set) {
	for (std::string t: Set) {
		errs() << t << "\n";
	}
	return;
}

void CallGraphPass::ExhaustiveSearch4FriendTypes(std::string Search4Type) {
	// Find friend types step 1:
	// List all sub-types using form: head-body-tail.
	// SplitArray[i][0]: head
	// SplitArray[i][1]: body
	// SplitArray[i][2]: tail
	// Body can be replaced by friend types.
	//errs() << "Looking for sub-types..." << "\n";
	list<std::string> MLTyList;
	MLTyList = MLTypeName2List(Search4Type);
	LayerNumSet.insert(MLTyList.size());
	LayerNumArray[MLTyList.size()-1]++;

	int LayerNum = MLTyList.size();
	std::string MLTyArray[LayerNum];
	for (int layer = 0; layer < LayerNum; layer++) {
		MLTyArray[layer] = MLTyList.front();
		MLTyList.pop_front();
	}

	int ArraySize = (1 + LayerNum) * LayerNum / 2;
	std::string SplitArray[ArraySize][3];
	std::string head, body, tail;
		
	int i = 0;
	for (int LayerSpan = 0; LayerSpan < LayerNum; LayerSpan++) {
		for (int BodyIdx = 0; BodyIdx < LayerNum-LayerSpan; BodyIdx++) {
			head = "";
			body = "";
			tail = "";
		
			if (BodyIdx != 0) {
				for (int j = 0; j < BodyIdx; j++) {
					head += MLTyArray[j] + "|";
				}
			}
			SplitArray[i][0] = head;
			
			int BodyEnd = BodyIdx + LayerSpan;
			for (int k = BodyIdx; k <= BodyEnd; k++) {
				body += MLTyArray[k] + "|";
			}	
			body.pop_back();		
			SplitArray[i][1] = body;
			
			int TailIdx = BodyIdx+LayerSpan+1;
			if (TailIdx < LayerNum) {
				for (int l = TailIdx; l < LayerNum; l++) {
					tail += "|" + MLTyArray[l];
				}
			}
			SplitArray[i][2] = tail;
			 
			i++;
		}
	}

	// Find friend types step 2:
	// Find friend types for each sub-type (body)
	// Concatenate sub-type's friend types with sub-type's head and tail
	// to generate friend types for the entire multi-layer type
	StrSet TS; // Search4Type's friend type set
	StrSet bodyTS;
	StrSet SFSet; // Sub-type's friend type set		
	std::string FriendType;
	bool first;	
	for (int Aidx = 0; Aidx < ArraySize; Aidx++) {
		head = SplitArray[Aidx][0];
		body = SplitArray[Aidx][1]; // Look for body's friend types
		tail = SplitArray[Aidx][2];
		bodyTS.clear();
	
		// Remove the idx of the first-layer in body
		list<std::string> bodyList;
		bodyList = MLTypeName2List(body);
		std::string bodyFst = bodyList.front();
		std::string bodyIdx;
		std::string bodyRaw;
	
		if ((i = bodyFst.find("#")) != string::npos) {
			bodyIdx = bodyFst.substr(i);
			bodyRaw = body;
			bodyRaw.erase(i, bodyIdx.length());
		}
		else {
			bodyIdx = "";
			bodyRaw = body;
		}
				
		if (NewTypeRelationshipMap.find(bodyRaw) != NewTypeRelationshipMap.end()) {
			SFSet = NewTypeRelationshipMap[bodyRaw];
		}
		else {
			if (Aidx == 0) {
				first = true;
			}
			else {
				first = false;
			}
			SFSet = UpgradeTypeRelationshipMap(bodyRaw, first);
		}
			
		if (SFSet.empty()) { // current body has no friend type
			FriendType = head + body + tail;
			bodyTS.insert(FriendType);
		}
		else {
			for (StrSet::iterator SFit = SFSet.begin(); SFit != SFSet.end(); SFit++) {
				// If the friend type is a fuzzy type, do not put back idx
				if (*SFit == "&") {
					FriendType = head + "&" + tail;
					bodyTS.clear();
					bodyTS.insert(FriendType);
					break;
				}
				
				// Put back the idx of the first-layer in body
				bodyList.clear();
				bodyList = MLTypeName2List(*SFit);
				bodyFst = bodyList.front() + bodyIdx;
				bodyList.pop_front();
				std::string newBody = bodyFst;
				while (!bodyList.empty()) {
					newBody += "|" + bodyList.front();
					bodyList.pop_front();
				}

				// Generate friend type for the entire multi-layer type				
				FriendType = head + newBody + tail;		
				bodyTS.insert(FriendType);
			}
		}
		TS.insert(bodyTS.begin(), bodyTS.end());
	}
		
	// Trim TS, remove invalid types
	std::set<std::string> TS_copy;
	for (std::string ft: TS) {
		if (IsValidType(ft)) {
			TS_copy.insert(ft);
		}		
	}		
	TS = TS_copy;	
				
	// Remove "&" at the end to generate corresponding non-fuzzy types
	std::string clearType;
	for (std::string ft: TS) {
		if (ft.substr(ft.length()-1,1) == "&") {
			clearType = ft.substr(0, ft.length()-2);
			TS_copy.insert(clearType);
		}
	}
	TS.insert(TS_copy.begin(), TS_copy.end()); 
	
	// Update FriendTyMap			
	if (FriendTyMap.find(Search4Type) == FriendTyMap.end()) {
		FriendTyMap[Search4Type] = TS;
	}
	else {
		FriendTyMap[Search4Type].insert(TS.begin(), TS.end());
	}
	
	return;
}

void CallGraphPass::FindCalleesWithSMLTA(CallInst *CI) {
	
	std::string MLTypeName; // Call site's type
	FuncSet FS;
	
	// Get Caller's multi-layer type
	Value *CV = CI->getCalledOperand();
	Type *LayerTy = CV->getType();
	MLTypeName = SingleType2String(LayerTy);
	
	if (isClassType(MLTypeName)) {
		MLTypeName = GenClassTyName(MLTypeName);
		// Deal with virtual functions
		auto ai = CI->arg_begin();
		Value *arg0 = *ai;
		if (LoadInst *LI = dyn_cast<LoadInst>(arg0)) {
			Value *LIop = LI->getOperand(0);
			if (DerivedClassMap.find(LIop) != DerivedClassMap.end()) {
				std::string classTyName = DerivedClassMap[LIop];
				int index = MLTypeName.find("|");
				MLTypeName = MLTypeName.substr(0, index+1) + classTyName;
			}
		}
		FS = MLTypeFuncMap[stringHash(MLTypeName)];
			
	}
	else {	
		MLTypeName = GenerateMLTypeName(CI, CV, MLTypeName);
	
		// If the called value can be traced back to an AllocaInst in ArgAllocSet
		// This call site has incomplete type, use "&" to mark it
		if (AIFlag == true) {
			AIFlag = false; // turn off AIFlag
			if (ArgAllocaSet.find(RecordAI) != ArgAllocaSet.end()) {
				MLTypeName += "|&";
			}
		}
	}	

	if (TargetLookupMap.find(stringHash(MLTypeName)) != TargetLookupMap.end()) {
		FS = TargetLookupMap[stringHash(MLTypeName)];
	}		
	else {
		// Initialize FS, it will be enlarged later
		FS = MLTypeFuncMap[stringHash(MLTypeName)];
		ExhaustiveSearch4FriendTypes(MLTypeName);		
		StrSet FTySet;
		StrSet FriendTySetRound1;
		StrSet FriendTySetRound2;
		StrSet FriendTySetRound3;

		//FriendTySetRound1 = FriendTyMap[MLTypeName];
		//for (auto f: FriendTySetRound1) {
		//	ExhaustiveSearch4FriendTypes(f);
		//	FriendTySetRound2.insert(FriendTyMap[f].begin(), FriendTyMap[f].end());
			//FriendTySetRound2 = FriendTyMap[f];
			//FriendTyMap[MLTypeName].insert(FriendTySetRound2.begin(), FriendTySetRound2.end());
		//}
		//FriendTyMap[MLTypeName].insert(FriendTySetRound2.begin(), FriendTySetRound2.end());
		
		FTySet = FriendTyMap[MLTypeName];
		//errs() << "friend type set size: " << FTySet.size() << "\n";
						
		// Search for matched types for each element in FTySet
		StrSet MatchedTySet;	// Types match with CheckType
		StrSet AllMatchedTySet;	// Types match with all elements in FTySet	
		StrSet CumuTySet;	
		for (std::set<std::string>::iterator TSit = FTySet.begin(); TSit != FTySet.end(); TSit++) {
			std::string CheckType = *TSit;
			list<std::string> CheckTyList;
			CheckTyList = MLTypeName2List(CheckType);
			//errs() << "checktype: " << CheckType << "\n";
			
			// Initialize the sets
			MatchedTySet.clear();
			CumuTySet.clear();
			
			std::string FirstLayer = CheckTyList.front();
			CheckTyList.pop_front();
			CumuTySet.insert(FirstLayer);

#ifdef DTnocache
			goto NOCACHE;
#endif
		
			// Lookup the cache for matched types
			MatchedTySet = MatchedTyMap[stringHash(CheckType)];
			if (!MatchedTySet.empty()) {
				AllMatchedTySet.insert(MatchedTySet.begin(), MatchedTySet.end());
				continue;
			}
			
			NOCACHE:
			if (CheckTyList.empty()) {	// Single-layer type
				//errs() << "single-layer " << "\n";
				MatchedTySet = AddFuzzyTypeAndCopySet(CumuTySet);
				AllMatchedTySet.insert(MatchedTySet.begin(), MatchedTySet.end());
			}
			else {	// >= 2 layers
				std::string SecondLayer = CheckTyList.front();
				if (SecondLayer == "&") {
					MatchedTySet = CoverAll(CumuTySet, 1);
					AllMatchedTySet.insert(MatchedTySet.begin(), MatchedTySet.end());
					MatchedTyMap[stringHash(CheckType)] = MatchedTySet;
					continue;
				}
				CheckTyList.pop_front();				
				CumuTySet = UpdateCumuTySet(CumuTySet, 1, SecondLayer);
				
				if (CheckTyList.empty()) {	// Two-layer type
					MatchedTySet = AddFuzzyTypeAndCopySet(CumuTySet);
					AllMatchedTySet.insert(MatchedTySet.begin(), MatchedTySet.end());
				}
				else { // >= 3 layers
					std::string ThirdLayer = CheckTyList.front();
					if (ThirdLayer == "&") {
						MatchedTySet = CoverAll(CumuTySet, 2);
						AllMatchedTySet.insert(MatchedTySet.begin(), MatchedTySet.end());
						MatchedTyMap[stringHash(CheckType)] = MatchedTySet;
						continue;
					}
					CheckTyList.pop_front();
					CumuTySet = UpdateCumuTySet(CumuTySet, 2, ThirdLayer);
					
					if (CheckTyList.empty()) {	// Three-layer type
						MatchedTySet = AddFuzzyTypeAndCopySet(CumuTySet);
						AllMatchedTySet.insert(MatchedTySet.begin(), MatchedTySet.end());
					}
					else {	// >= 4 layers
						std::string FourthLayer = CheckTyList.front();
						if (FourthLayer == "&") {
							MatchedTySet = CoverAll(CumuTySet, 3);
							AllMatchedTySet.insert(MatchedTySet.begin(), MatchedTySet.end());
							MatchedTyMap[stringHash(CheckType)] = MatchedTySet;
							continue;
						}
						CheckTyList.pop_front();
						CumuTySet = UpdateCumuTySet(CumuTySet, 3, FourthLayer);
						
						if (CheckTyList.empty()) {	// Four-layer type
							MatchedTySet = AddFuzzyTypeAndCopySet(CumuTySet);
							AllMatchedTySet.insert(MatchedTySet.begin(), MatchedTySet.end());
						}
						else {	// >= 5 layers
							std::string FifthLayer = CheckTyList.front();
							if (FifthLayer == "&") {
								MatchedTySet = CoverAll(CumuTySet, 4);
								AllMatchedTySet.insert(MatchedTySet.begin(), MatchedTySet.end());
								MatchedTyMap[stringHash(CheckType)] = MatchedTySet;
								continue;
							}
							CheckTyList.pop_front();
							CumuTySet = UpdateCumuTySet(CumuTySet, 4, FifthLayer);
							
							if (CheckTyList.empty()) {	// Five-layer type
								MatchedTySet = AddFuzzyTypeAndCopySet(CumuTySet);
								AllMatchedTySet.insert(MatchedTySet.begin(), MatchedTySet.end());
							}
							else {	// >= 6 layers
								std::string SixthLayer = CheckTyList.front();
								if (SixthLayer == "&") {
									MatchedTySet = CoverAll(CumuTySet, 5);
									AllMatchedTySet.insert(MatchedTySet.begin(), MatchedTySet.end());
									MatchedTyMap[stringHash(CheckType)] = MatchedTySet;
									continue;
								}						
								CheckTyList.pop_front();
								CumuTySet = UpdateCumuTySet(CumuTySet, 5, SixthLayer);
								
								if (CheckTyList.empty()) {	// Six-layer type
									MatchedTySet = AddFuzzyTypeAndCopySet(CumuTySet);
									AllMatchedTySet.insert(MatchedTySet.begin(), MatchedTySet.end());
								}
								else {	// >= 7 layers
									std::string SeventhLayer = CheckTyList.front();
									if (SeventhLayer == "&") {
										MatchedTySet = CoverAll(CumuTySet, 6);
										AllMatchedTySet.insert(MatchedTySet.begin(), MatchedTySet.end());
										MatchedTyMap[stringHash(CheckType)] = MatchedTySet;
										continue;
									}	
									CheckTyList.pop_front();
									CumuTySet = UpdateCumuTySet(CumuTySet, 6, SeventhLayer);
									
									if (CheckTyList.empty()) {	// Seven-layer type
										MatchedTySet = AddFuzzyTypeAndCopySet(CumuTySet);
										AllMatchedTySet.insert(MatchedTySet.begin(), MatchedTySet.end());
									}
									else {	// > 7 layers, not in scope
										std::string OuterLayer = "&";
										CheckTyList.pop_front();
										CumuTySet = UpdateCumuTySet(CumuTySet, 7, OuterLayer);
										MatchedTySet = AddFuzzyTypeAndCopySet(CumuTySet);
										AllMatchedTySet.insert(MatchedTySet.begin(), MatchedTySet.end());
									}
								}
							}
						}
					}
				}
			}
			MatchedTyMap[stringHash(CheckType)] = MatchedTySet;
		}
		
		
		// Lookup MLTypeFuncMap
		//errs() << "all matched types" << "\n";
		FuncSet FStmp;
		for (std::string t: AllMatchedTySet) {
			//errs() << t << "\n";
			FStmp = MLTypeFuncMap[stringHash(t)];
			FS = FSMerge(FS, FStmp);
			FStmp.clear();
		}

		
#ifdef DTweak
		FS.clear();

		// Traverse all matched types
		for (std::string mt: AllMatchedTySet) {
			FuncSet FS1, FS2;
			std::list<std::string> typeList;
			typeList = MLTypeName2List(mt);
			
			std::string LayTy = typeList.front();
			typeList.pop_front();
			FS1 = WMLTATypeFuncMap[stringHash(LayTy)];
			
			while (!typeList.empty()) {
				LayTy = typeList.front();
				typeList.pop_front();
				FS2 = WMLTATypeFuncMap[stringHash(LayTy)];
				FS1 = FSIntersect(FS1, FS2);
			}
			
			FS.insert(FS1.begin(), FS1.end());
		}

/*		
		// Traverse all friend types
		for (std::string t: FTySet) {
			FuncSet FS1, FS2, FS3;
			std::list<std::string> typeList;
			typeList = MLTypeName2List(t);

			// Get function set of the 1st-layer
			std::string LayTy = typeList.front();
			typeList.pop_front();
			FS1 = WMLTATypeFuncMap[stringHash(LayTy)];
			
			
			// Get function set of the other layers
			while (!typeList.empty()) {
				LayTy = typeList.front();
				typeList.pop_front();
				
				if (LayTy == "&") {continue;}
				FS2 = WMLTATypeFuncMap[stringHash(LayTy)];		
				FS1 = FSIntersect(FS1, FS2);			
			}
		
			// Merge function sets of all matched types
			FS.insert(FS1.begin(), FS1.end());
		}
*/
#endif
 	
		// Remove the functions that are not address-taken
		FuncSet ATFS;
		for (auto F: FS) {
			if (F->hasAddressTaken()) {
				ATFS.insert(F);
			}
		}
		FS = ATFS;
		
		// Record in TargetLoopupMap;
	}
	
			
	// Statistics
	errs() << "\n";
	PrintResults(CI, FS, MLTypeName);
	//errs() << FS.size() << "\n";
		
	Ctx->NumIndirectCallTargets += FS.size();
	
	if (FS.size() == 0) {
		Ctx->NoTargetCalls++;
	}
	else if (FS.size() >= 1 && FS.size() < 2 ) {
		Ctx->ZerotTargetCalls++;
	}
	else if (FS.size() >= 2 && FS.size() < 4 ) {
		Ctx->OnetTargetCalls++;
	}
	else if (FS.size() >= 4 && FS.size() < 8 ) {
		Ctx->TwotTargetCalls++;
	}
	else if (FS.size() >= 8 && FS.size() < 16 ) {
		Ctx->ThreetTargetCalls++;
	}
	else if (FS.size() >= 16 && FS.size() < 32 ) {
		Ctx->FourtTargetCalls++;
	}
	else if (FS.size() >= 32 && FS.size() < 64 ) {
		Ctx->FivetTargetCalls++;
	}
	else if (FS.size() >= 64 && FS.size() < 128 ) {
		Ctx->SixtTargetCalls++;
	}
	else if (FS.size() >= 128 && FS.size() < 256 ) {
		Ctx->SeventTargetCalls++;
	}
	else if (FS.size() >= 256 ) {
		Ctx->EighttTargetCalls++;
	}


	return;
}

void CallGraphPass::PrintMaps() {

	errs() << "========== MLTypeFuncMap ==========" << "\n";
	DenseMap<size_t, FuncSet>::iterator mapiter;
	mapiter = MLTypeFuncMap.begin();
	while (mapiter != MLTypeFuncMap.end()) {
		errs() << "type: " << ReferMap[mapiter->first] << "\n";
		FuncSet FS = mapiter->second;
		for (auto F: FS) {
			errs() << "target: " << F->getName() << "\n";
		}
		mapiter++;
	}
	
	errs() << "========== WMLTypeFuncMap ==========" << "\n";
	DenseMap<size_t, FuncSet>::iterator wmapiter;
	wmapiter = WMLTATypeFuncMap.begin();
	while (wmapiter != WMLTATypeFuncMap.end()) {
		errs() << "type: " << wmapiter->first << "\n";
		FuncSet FS = wmapiter->second;
		for (auto F: FS) {
			errs() << "target: " << F->getName() << "\n";
		}
		wmapiter++;
	}
	
	errs() << "========== TypeRelationshipMap ==========" << "\n";
	std::map<std::string, StrSet>::iterator iter;
	iter = TypeRelationshipMap.begin();
	while (iter != TypeRelationshipMap.end()) {
		errs() << "type: " << iter->first << "\n";
		StrSet ss = iter->second;
		for (auto str: ss) {
			errs() << "friend: " << str << "\n";
		}
		iter++;
	}
	
	errs() << "========== EscapingSet ==========" << "\n";
	for (auto s: EscapingSet) {
		errs() << "Escaping type: " << s << "\n";
	}

	return;
}


// Remove cycles between types in TypeRelationshipMap
// Store the organized type-relationships in NewTypeRelationshipMap
StrSet CallGraphPass::UpgradeTypeRelationshipMap(std::string key, bool first) {
	StrSet S1, S2, S3;
	int S1_origSize;
	int S1_newSize;

	S1_origSize = 0;
	S1_newSize = 0;
	
	// Find friend types for the current key
	S2.insert(key);
	int loop = 0;
	while(!S2.empty()) {
		loop++;
		for (StrSet::iterator it2 = S2.begin(); it2 != S2.end(); it2++) {
			std::string S2Ele = *it2;
			StrSet S2Friend = TypeRelationshipMap[S2Ele];
			for (std::string ft: S2Friend) {				
				if (S2Ele.substr(0,6) == "struct" 
				|| S2Ele.substr(0,5) == "array" 
				|| S2Ele.substr(0,6) == "vector") {
					if (ft.substr(0,6) == "struct"
					|| ft.substr(0,5) == "array"
					|| ft.substr(0,6) == "vector"
					|| ft.substr(0,5) == "union") {
						S3.insert(ft);
					}
				}
				else {
					if (!IsUnsupportedTypeStr(ft)) {
						S3.insert(ft);
					}
				}				
			}
		}
		
		S1_origSize = S1.size();
		S1 = TypeSetMerge(S1, S2);
		S1_newSize = S1.size();
		
		if (loop > 1) {
			for (auto e: S1) {
				if (IsEscapingType(e)) {
					return S1;
				} 
			}
		}
		
		if (S1_origSize == S1_newSize) {
			// All friends of this key is in S1
			break;
		}
		else {
			// Continue to find new friend types
			S2.empty();
			S2 = S3;
			S3.empty();
		}
	}
	
	// Store to NewTypeRelationshipMap
	NewTypeRelationshipMap[key] = S1;
				

	return S1;
}

bool CallGraphPass::IdentifyTargets(Module *M) {
	
	//PrintMaps();
	//errs() << "size: " << MLTypeFuncMap.size() << "\n";

	errs() << "Identify indirect call targets with SMLTA..." << "\n";	
	for (Module::iterator f = M->begin(), fe = M->end(); f != fe; ++f) {		
		Function *F = &*f;
		
		// Skip dead functions
		if (F->use_empty() && (F->getName().str() != "main")) {
			continue;
		}

		DerivedClassMap.clear();		
		for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {			
			if (StoreInst *SI = dyn_cast<StoreInst>(&*i)) {
				Value *VO = SI->getValueOperand();
				Value *PO = SI->getPointerOperand();
				if (CastInst *CastI = dyn_cast<CastInst>(VO)) {
					Type *SrcTy = CastI->getSrcTy();
					std::string SrcTyName = SingleType2String(SrcTy);
					if (SrcTyName.substr(0,6) == "class.") {
						SrcTyName = StripClassType(SrcTyName);
						DerivedClassMap[PO] = SrcTyName;
					}
				}
			}		
			if (CallInst *CI = dyn_cast<CallInst>(&*i)) {
				// Indirect call
				if (CI->isIndirectCall()) { 
					FindCalleesWithSMLTA(CI);

					// Record indirect calls
					Ctx->IndirectCallInsts.push_back(CI);
				}
				// Direct call
				else {
					// Not goal of this work
				}
			}
		}
	}
	
	//for (auto it=LayerNumSet.cbegin(); it!=LayerNumSet.cend(); it++) {
	//	errs() << *it << " ";
	//}
	//errs() << "LayerNumArray: " << "\n";
	//for (int i=0; i<12; i++) {
	//	errs() << LayerNumArray[i] << " ";
	//}
	
	return false;
}
