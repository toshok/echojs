TOP=..

include $(TOP)/build/config.mk

COFFEE_SOURCES=				\
	nodevisitor.coffee		\
	compiler.coffee			\
	debug.coffee			\
	echo-util.coffee		\
	errors.coffee			\
	closure-conversion.coffee	\
	optimizations.coffee		\
	types.coffee			\
	consts.coffee			\
	exitable-scope.coffee		\
	runtime.coffee			\
	terminal.coffee			\
	module-resolver.coffee

JS_SOURCES=				\
	map.js				\
	set.js				\
	stack.js

DESTDIR = generated

GENERATED_FILES=$(COFFEE_SOURCES:%.coffee=$(DESTDIR)/%.js) $(JS_SOURCES:%.js=$(DESTDIR)/%.js)

all-local:: $(GENERATED_FILES)

clean-local::
	rm -f $(GENERATED_FILES)

$(DESTDIR)/%.js: %.coffee
	coffee -m -o $(DESTDIR) -c $<

$(DESTDIR)/%.js: %.js
	cp $< $@

include $(TOP)/build/build.mk
