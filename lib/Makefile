TOP=..

include $(TOP)/build/config.mk

ES6_SOURCES=			 	\
	abi.js				\
	sret-abi.js			\
	ast-builder.js			\
	node-visitor.js			\
	compiler.js			\
	common-ids.js			\
	closure-conversion.js		\
	debug.js			\
	echo-util.js			\
	errors.js			\
	optimizations.js		\
	types.js			\
	consts.js			\
	exitable-scope.js		\
	runtime.js			\
	module-info.js			\
	set-es6.js			\
	stack-es6.js			\
	host-config.js			\
	passes/desugar-arguments.js	\
	passes/desugar-arrow-functions.js \
	passes/desugar-classes.js	\
	passes/desugar-defaults.js	\
	passes/desugar-destructuring.js	\
	passes/desugar-for-of.js	\
	passes/desugar-generator-functions.js \
	passes/desugar-import-export.js	\
	passes/desugar-let-loopvars.js	\
	passes/desugar-metaproperties.js \
	passes/desugar-rest-parameters.js \
	passes/desugar-spread.js	\
	passes/desugar-templates.js	\
	passes/desugar-update-assignments.js \
	passes/eq-idioms.js		\
	passes/func-decls-to-vars.js	\
	passes/gather-imports.js	\
	passes/hoist-func-decls.js	\
	passes/hoist-vars.js		\
	passes/iife-idioms.js		\
	passes/lambda-lift.js		\
	passes/name-anonymous-functions.js \
	passes/new-cc.js		\
	passes/replace-unary-void.js	\
	passes/substitute-variables.js

DESTDIR = generated

GENERATED_EXTERNAL_FILES = \
	$(DESTDIR)/ejs-es6.js \
	$(DESTDIR)/external-deps/esprima/esprima-es6.js \
	$(DESTDIR)/external-deps/escodegen/escodegen-es6.js \
	$(DESTDIR)/external-deps/estraverse/estraverse-es6.js \
	$(DESTDIR)/external-deps/esutils/esutils-es6.js \
	$(DESTDIR)/external-deps/esutils/lib/code.js \
	$(DESTDIR)/external-deps/esutils/lib/ast.js \
	$(DESTDIR)/external-deps/esutils/lib/keyword.js

GENERATED_FILES=$(ES6_SOURCES:%.js=$(DESTDIR)/lib/%.js)

all-local:: $(GENERATED_FILES) $(GENERATED_EXTERNAL_FILES)

dist-local:: $(GENERATED_FILES)

clean-local::
	rm -rf $(DESTDIR) host-config.js

BABEL_SED_REPLACEMENTS= -e s,\"@llvm\",\"llvm\", \
			-e s,\'@llvm\',\'llvm\', \
			-e s,@node-compat/,,

BABEL_ARGS= --presets es2015
BABEL=babel

$(DESTDIR)/ejs-es6.js: ../ejs-es6.js
	@echo [babel] $<
	@mkdir -p `dirname $@`
	@$(BABEL) $(BABEL_ARGS) -o "" $< | sed $(BABEL_SED_REPLACEMENTS) > $@

$(DESTDIR)/external-deps/esprima/esprima-es6.js: ../external-deps/esprima/esprima-es6.js
	@echo [babel] $<
	@mkdir -p `dirname $@`
	@$(BABEL) $(BABEL_ARGS) -o "" $< | sed $(BABEL_SED_REPLACEMENTS) > $@

$(DESTDIR)/external-deps/escodegen/escodegen-es6.js: ../external-deps/escodegen/escodegen-es6.js
	@echo [babel] $<
	@mkdir -p `dirname $@`
	@$(BABEL) $(BABEL_ARGS) -o "" $< | sed $(BABEL_SED_REPLACEMENTS) > $@

$(DESTDIR)/external-deps/estraverse/estraverse-es6.js: ../external-deps/estraverse/estraverse-es6.js
	@echo [babel] $<
	@mkdir -p `dirname $@`
	@$(BABEL) $(BABEL_ARGS) -o "" $< | sed $(BABEL_SED_REPLACEMENTS) > $@

$(DESTDIR)/external-deps/esutils/esutils-es6.js: ../external-deps/esutils/esutils-es6.js
	@echo [babel] $<
	@mkdir -p `dirname $@`
	@$(BABEL) $(BABEL_ARGS) -o "" $< | sed $(BABEL_SED_REPLACEMENTS) > $@

$(DESTDIR)/external-deps/esutils/lib/%.js: ../external-deps/esutils/lib/%.js
	@echo [babel] $<
	@mkdir -p `dirname $@`
	@$(BABEL) $(BABEL_ARGS) -o "" $< | sed $(BABEL_SED_REPLACEMENTS) > $@

$(DESTDIR)/lib/%.js: %.js
	@echo [babel] $<
	@mkdir -p `dirname $@`
	@$(BABEL) $(BABEL_ARGS) -o "" $< | sed $(BABEL_SED_REPLACEMENTS) > $@

%.js: %.js.in
	@echo [gen] $@ && (cat $< | sed -e s,@LLVM_SUFFIX@,$(LLVM_SUFFIX),g -e s,@RUNLOOP_IMPL@,$(EJS_RUNLOOP_IMPL),g > $@)

.PRECIOUS: host-config.js

include $(TOP)/build/build.mk
