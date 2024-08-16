# DeepType: Refining Indirect Call Targets with Strong Multi-layer Type Analysis
Considering the high false positive rate of traditional type-based analysis, we use multi-layer type to describe the type of a function pointer, which consists of function signature along with the composite types holding it. However, multi-layer type introduces challenges in type matching because address-taken functions may be propagated between multi-layer types through information flow, making it hard to collect all potential targets. The original paper of Multi-Layer Type Analysis (MLTA) bypasses the challenges by splitting multi-layer types, which weakens the restrictions provided by multi-layer types, thereby negatively affecting accuracy. 

We proposed an advanced approach, Strong Multi-Layer Type Analysis (SMLTA), to mitigate the false positive targets produced by MLTA. SMLTA adheres to the strong restriction that identifies only those functions as targets whose entire multi-layer types match with the indirect calls. SMLTA addresses the challenges in multi-layer type matching by resolving the relationships between multi-layer types based on the directions of information flow, and utilizes an adapted breadth- first search (BFS) algorithm to discover all multi-layer types engaged in the propagation of target functions. It also employs a conservative strategy to deal with ambiguous type information due to information flow.

DEEPTYPE is a prototype implementation of SMLTA, which overcomes challenges in multi-layer type matching and utilizes SMLTA to precisely and efficiently identify indirect call targets. It is built on LLVM 15.0 and is tested on Ubuntu 20.04. 

## Setup Guide
### Build LLVM
```
$ git clone -b release/15.x https://github.com/llvm/llvm-project.git
$ cd /root/of/llvm/project
$ mkdir build
$ cd build
$ cmake -DLLVM_TARGET_ARCH="X86" -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="clang;lldb;lld" -DLLVM_TARGETS_TO_BUILD="ARM;X86;AArch64" -G "Unix Makefiles" ../llvm
$ cmake --build .
```
### Build DeepType
1. Set the path of ```LLVM_BUILD``` in Makefile at line 2
2. Compile DeepType
```
$ cd /root/of/DeepType
$ make
```

## How to Use
The executable file takes bitcode(s) as argument(s). 
```
$ cd root/of/DeepType/build/lib
$ ./kanalyzer filename.bc
```
Although DeepType supports all optimization levels, it has the best precision at O0 optimization level. When compiling the target program into bitcode, use flags ```-g -Xclang -no-opaque-pointers``` to include debugging information and disable opaque pointer mode.

## Analysis Results
DeepType outputs the following information of the analyzed program:  
1. A list of indirect calls along with their respective targets.
2. Total number of indirect calls and indirect call targets.
3. Average number of indirect call targets (ANT).
4. Execution time.
   
## Configurations
To evaluate DeepType comprehensively, we developed 3 variants of DeepType: DT-weak, DT-noSH, DT-nocache.

**DT-weak** stores splitted multi-layer types to help examine the impact of recording entire multi-layer types. To reproduce the experiments, uncomment ```#define DTweak``` in ```/DeepType/src/lib/CallGraph.cc``` and recompile DeepType.
```
#define DTweak
//#define DTnoSH
//#define DTnocache
```

**DT-noSH** disables the special handlings in DeepType to reveal the contribution of SMLTA. To reproduce the experiments, uncomment ```#define DTnoSH``` in ```/DeepType/src/lib/CallGraph.cc``` and recompile DeepType.
```
//#define DTweak
#define DTnoSH
//#define DTnocache
```

**DT-nocache** disables the cache used in DeepType to measure the runtime overhead of DeepType without cache. To reproduce the experiments, uncomment ```#define DTnocache``` in ```/DeepType/src/lib/CallGraph.cc``` and recompile DeepType.
```
//#define DTweak
//#define DTnoSH
#define DTnocache
```

## Benchmarks
The bitcode of the benchmarks in our paper is available at: https://drive.google.com/file/d/1U9rMr4UC0uxVhAH7p0R3127lJpaaQMuj/view?usp=sharing.

## Publication
This project is the artifact of the paper DEEPTYPE: Refining Indirect Call Targets with Strong Multi-layer Type Analysis, which is accepted at the 33rd USENIX Security Symposium (USENIX 2024).
```
@inproceedings{xia:deeptype,
  title        = {{DEEPTYPE: Refining Indirect Call Targets with Strong Multi-layer Type Analysis}},
  author       = {Tianrou Xia and Hong Hu and Dinghao Wu},
  booktitle    = {Proceedings of the 33rd USENIX Security Symposium (USENIX 2024)},
  month        = {aug},
  year         = {2024},
  address      = {Philadelphia, PA},
}
```
