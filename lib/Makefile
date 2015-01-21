TOP=..

include $(TOP)/build/config.mk

COFFEE_SOURCES=				\
	ast-builder.coffee		\
	nodevisitor.coffee		\
	compiler.coffee			\
	debug.coffee			\
	echo-util.coffee		\
	errors.coffee			\
	closure-conversion.coffee	\
	new-cc.coffee			\
	optimizations.coffee		\
	types.coffee			\
	consts.coffee			\
	exitable-scope.coffee		\
	runtime.coffee			\
	terminal.coffee			\
	module-resolver.coffee		\
	host-config.coffee

JS_SOURCES=				\
	map.js				\
	set.js				\
	stack.js

DESTDIR = generated

GENERATED_FILES=$(COFFEE_SOURCES:%.coffee=$(DESTDIR)/%.js) $(JS_SOURCES:%.js=$(DESTDIR)/%.js)

all-local:: $(GENERATED_FILES) host-config-es6.js

clean-local::
	rm -f $(GENERATED_FILES) host-config-es6.js host-config.coffee

$(DESTDIR)/%.js: %.coffee
	@echo [coffee] $@ && coffee -m -o $(DESTDIR) -c $<

$(DESTDIR)/%.js: %.js
	@echo [copy] $@ && cp $< $@

%.coffee: %.coffee.in
	@echo [gen] $@ && (cat $< | sed -e s,@LLVM_SUFFIX@,$(LLVM_SUFFIX),g > $@)

%: %.in
	@echo [gen] $@ && (cat $< | sed -e s,@LLVM_SUFFIX@,$(LLVM_SUFFIX),g > $@)

include $(TOP)/build/build.mk
