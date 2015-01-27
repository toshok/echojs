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
	ejs.coffee

JS_SOURCES=				\
	map.js				\
	set.js				\
	stack.js

DESTDIR = generated

GENERATED_FILES=$(COFFEE_SOURCES:%.coffee=$(DESTDIR)/%.js) $(JS_SOURCES:%.js=$(DESTDIR)/%.js)

all-local:: $(GENERATED_FILES) $(DESTDIR)/host-config.js host-config-es6.js

dist-local:: $(GENERATED_FILES)

clean-local::
	rm -f host-config-es6.js $(DESTDIR)/host-config.js

$(DESTDIR)/host-config.js: host-config.js.in
	@echo [gen] $@ && (cat $< | sed -e s,@LLVM_SUFFIX@,$(LLVM_SUFFIX),g > $@)

$(DESTDIR)/%.js: %.coffee
	@echo [coffee] $@ && coffee -m -o $(DESTDIR) -c $<

$(DESTDIR)/%.js: %.js
	@echo [copy] $@ && cp $< $@

%.coffee: %.coffee.in
	@echo [gen] $@ && (cat $< | sed -e s,@LLVM_SUFFIX@,$(LLVM_SUFFIX),g > $@)

%: %.in
	@echo [gen] $@ && (cat $< | sed -e s,@LLVM_SUFFIX@,$(LLVM_SUFFIX),g > $@)

include $(TOP)/build/build.mk
