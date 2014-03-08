TOP=$(shell pwd)/../..

include $(TOP)/build/config.mk

LLVM_LINK_FLAGS = $(shell llvm-config$(LLVM_SUFFIX) --ldflags --libs)

JS_SOURCES=$(filter-out ejs.js,$(wildcard *.js))
EXTRA_JS_SOURCES= ../../esprima/esprima.js ../../escodegen/escodegen.js ../../estraverse/estraverse.js

make-stage1: $(wildcard *.js) ejs.js
	@echo "****************************************************"
	@echo "****************************************************"
	@echo "Building stage 1"
	@echo "****************************************************"
	@echo "****************************************************"
	@$(MAKE) -C ..
	time NODE_PATH=$(TOP)/node-llvm/build/Release:$(TOP)/lib/generated:$(TOP)/esprima:$(TOP)/escodegen:$(TOP)/estraverse ../../ejs --leave-temp --module "../../ejs-llvm/libejsllvm-module.a,llvm,_ejs_llvm_init,$(LLVM_LINK_FLAGS)" ejs.js $(JS_SOURCES) $(EXTRA_JS_SOURCES)

bootstrap::
	@$(MAKE) make-stage1 || exit 1
	@cp ejs.js.exe ../../ejs.js.exe.stage1
	@mv ejs.js.exe ../..

stages := $(shell seq 2 3)

define stage_template
make-stage$(1): $(wildcard *.js) ejs.js
	@echo "****************************************************"
	@echo "****************************************************"
	@echo "Building stage $(1)"
	@echo "****************************************************"
	@echo "****************************************************"
	@$(MAKE) -C ..
	time NODE_PATH=$(TOP)/node-llvm/build/Release:$(TOP)/lib/generated:$(TOP)/esprima:$(TOP)/escodegen:$(TOP)/estraverse ../../ejs.js.exe --leave-temp --module "../../ejs-llvm/libejsllvm-module.a,llvm,_ejs_llvm_init,$(LLVM_LINK_FLAGS)" ejs.js $(JS_SOURCES) $(EXTRA_JS_SOURCES)

bootstrap::
	@$(MAKE) make-stage$(1) || exit 1
	@cp ejs.js.exe ../../ejs.js.exe.stage$(1)
	@mv ejs.js.exe ../..
endef

bootstrap::

$(foreach stage,$(stages),$(eval $(call stage_template,$(stage))))

bootstrap::
	@echo "done, woohoo"

ejs.coffee: ../../ejs
	@cp ../../ejs ejs.coffee

ejs.js: ejs.coffee
	coffee -c ejs.coffee
