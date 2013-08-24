TOP=$(shell pwd)/../..

LLVM_LINK_FLAGS = \
  -L/usr/local/lib \
  -lpthread \
  -lm \
  -lLLVMMCDisassembler \
  -lLLVMXCoreCodeGen \
  -lLLVMXCoreAsmPrinter \
  -lLLVMXCoreInfo \
  -lLLVMSystemZCodeGen \
  -lLLVMSystemZAsmPrinter \
  -lLLVMSystemZInfo \
  -lLLVMSparcCodeGen \
  -lLLVMSparcAsmPrinter \
  -lLLVMSparcInfo \
  -lLLVMPowerPCCodeGen \
  -lLLVMPowerPCAsmPrinter \
  -lLLVMPowerPCInfo \
  -lLLVMPTXCodeGen \
  -lLLVMPTXAsmPrinter \
  -lLLVMPTXInfo \
  -lLLVMMipsAsmPrinter \
  -lLLVMMipsCodeGen \
  -lLLVMMipsInfo \
  -lLLVMMSP430CodeGen \
  -lLLVMMSP430Info \
  -lLLVMMSP430AsmPrinter \
  -lLLVMMBlazeDisassembler \
  -lLLVMMBlazeAsmParser \
  -lLLVMMBlazeCodeGen \
  -lLLVMMBlazeAsmPrinter \
  -lLLVMMBlazeInfo \
  -lLLVMLinker \
  -lLLVMipo \
  -lLLVMInterpreter \
  -lLLVMInstrumentation \
  -lLLVMJIT \
  -lLLVMExecutionEngine \
  -lLLVMCppBackend \
  -lLLVMCppBackendInfo \
  -lLLVMCellSPUCodeGen \
  -lLLVMCellSPUAsmPrinter \
  -lLLVMCellSPUInfo \
  -lLLVMCBackend \
  -lLLVMCBackendInfo \
  -lLLVMBlackfinCodeGen \
  -lLLVMBlackfinAsmPrinter \
  -lLLVMBlackfinInfo \
  -lLLVMBitWriter \
  -lLLVMX86Disassembler \
  -lLLVMX86AsmParser \
  -lLLVMX86CodeGen \
  -lLLVMX86AsmPrinter \
  -lLLVMX86Info \
  -lLLVMAsmParser \
  -lLLVMARMDisassembler \
  -lLLVMARMAsmParser \
  -lLLVMARMCodeGen \
  -lLLVMARMAsmPrinter \
  -lLLVMARMInfo \
  -lLLVMArchive \
  -lLLVMBitReader \
  -lLLVMAlphaCodeGen \
  -lLLVMSelectionDAG \
  -lLLVMAlphaAsmPrinter \
  -lLLVMAsmPrinter \
  -lLLVMMCParser \
  -lLLVMCodeGen \
  -lLLVMScalarOpts \
  -lLLVMInstCombine \
  -lLLVMTransformUtils \
  -lLLVMipa \
  -lLLVMAnalysis \
  -lLLVMTarget \
  -lLLVMMC \
  -lLLVMCore \
  -lLLVMAlphaInfo \
  -lLLVMSupport \
  -lLLVMSystem

JS_SOURCES=$(filter-out ejs.js,$(wildcard *.js))
EXTRA_JS_SOURCES= ../../esprima/esprima.js ../../escodegen/escodegen.js ../../estraverse/estraverse.js

bootstrap:
	@$(MAKE) make-stage1 || exit 1
	@cp ejs.js.exe ../../ejs.js.exe.stage1
	@mv ejs.js.exe ../..
	@$(MAKE) make-stage2 || exit 1
	@cp ejs.js.exe ../../ejs.js.exe.stage2
	@mv ejs.js.exe ../..
	@$(MAKE) make-stage3 || exit 1
	@cp ejs.js.exe ../../ejs.js.exe.stage3
	@mv ejs.js.exe ../..
	@echo "done, woohoo"

make-stage1: $(wildcard *.js) ejs.js
	@echo "****************************************************"
	@echo "****************************************************"
	@echo "Building stage 1 (compiling ejs-ejs with ejs-nodejs)"
	@echo "****************************************************"
	@echo "****************************************************"
	@$(MAKE) -C ..
	time NODE_PATH=$(TOP)/node-llvm/build/Release:$(TOP)/lib/generated:$(TOP)/esprima:$(TOP)/escodegen:$(TOP)/estraverse ../../ejs -d --leave-temp --module "../../ejs-llvm/libejsllvm-module.a,llvm,_ejs_llvm_init,$(LLVM_LINK_FLAGS)" ejs.js $(JS_SOURCES) $(EXTRA_JS_SOURCES)

make-stage2: $(wildcard *.js) ejs.js
	@echo "****************************************************"
	@echo "****************************************************"
	@echo "Building stage 2 (compiling ejs-ejs with ejs-ejs)"
	@echo "****************************************************"
	@echo "****************************************************"
	@$(MAKE) -C ..
	time `pwd`/../../ejs.js.exe -d --leave-temp --module "../../ejs-llvm/libejsllvm-module.a,llvm,_ejs_llvm_init,$(LLVM_LINK_FLAGS)" ejs.js $(JS_SOURCES) $(EXTRA_JS_SOURCES)

make-stage3: $(wildcard *.js) ejs.js
	@echo "****************************************************"
	@echo "****************************************************"
	@echo "Building stage 3 (compiling ejs-ejs with ejs-ejs)"
	@echo "****************************************************"
	@echo "****************************************************"
	@$(MAKE) -C ..
	time `pwd`/../../ejs.js.exe -d --leave-temp --module "../../ejs-llvm/libejsllvm-module.a,llvm,_ejs_llvm_init,$(LLVM_LINK_FLAGS)" ejs.js $(JS_SOURCES) $(EXTRA_JS_SOURCES)

ejs.coffee: ../../ejs
	@cp ../../ejs ejs.coffee

ejs.js: ejs.coffee
	coffee -c ejs.coffee
