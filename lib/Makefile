TOP=..

COFFEE_SOURCES=				\
	nodevisitor.coffee		\
	compiler.coffee			\
	debug.coffee			\
	echo-util.coffee		\
	closure-conversion.coffee	\
	optimizations.coffee		\
	types.coffee			\
	consts.coffee			\
	exitable-scope.coffee		\
	runtime.coffee			\
	terminal.coffee

JS_SOURCES=				\
	set.js				\
	stack.js

DESTDIR = generated

GENERATED_FILES=$(COFFEE_SOURCES:%.coffee=$(DESTDIR)/%.js) $(JS_SOURCES:%.js=$(DESTDIR)/%.js)

all-local:: $(GENERATED_FILES)

clean-local::
	rm -f $(GENERATED_FILES)

$(DESTDIR)/%.js: %.coffee
	coffee -o $(DESTDIR) -c $<

$(DESTDIR)/%.js: %.js
	cp $< $@

include $(TOP)/build/build.mk
