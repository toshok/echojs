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

bootstrap:
	@$(MAKE) make-stage1 || exit 1
	@mv ejs.js.exe ../..
	@$(MAKE) make-stage2 || exit 1
	@echo "done, woohoo"

make-stage1: $(wildcard *.js) ejs.js
	@echo "****************************************************"
	@echo "****************************************************"
	@echo "Building stage 1 (compiling ejs-ejs with ejs-nodejs)"
	@echo "****************************************************"
	@echo "****************************************************"
	@$(MAKE) -C ..
	NODE_PATH=$(TOP)/test/../node-llvm/build/Release:$(TOP)/test/../lib/coffee:$(TOP)/test/../lib:$(TOP)/test/../esprima:$(TOP)/test/../escodegen:$(TOP)/test/../estraverse ../../ejs --leave-temp --module "../../ejs-llvm/libejsllvm-module.a,llvm,_ejs_llvm_init,$(LLVM_LINK_FLAGS)" ejs.js $(JS_SOURCES) ../../esprima/esprima.js ../../escodegen/escodegen.js ../../estraverse/estraverse.js

make-stage2:
	@echo "****************************************************"
	@echo "****************************************************"
	@echo "Building stage 2 (compiling ejs-ejs with ejs-ejs)"
	@echo "****************************************************"
	@echo "****************************************************"
	@cp ../../ejs .
	@coffee -c ejs
	../../ejs.js.exe --leave-temp --module "../../ejs-llvm/libejsllvm-module.a,llvm,_ejs_llvm_init,$(LLVM_LINK_FLAGS)" ejs.js $(JS_SOURCES) ../../esprima/esprima.js ../../escodegen/escodegen.js ../../estraverse/estraverse.js

ejs.coffee: ../../ejs
	@cp ../../ejs ejs.coffee

ejs.js: ejs.coffee
	coffee -c ejs.coffee
