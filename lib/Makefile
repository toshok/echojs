TOP=..

COFFEE_FILES=				\
	nodevisitor.coffee		\
	compiler.coffee			\
	debug.coffee			\
	echo-util.coffee		\
	closure-conversion.coffee	\
	set.coffee			\
	stack.coffee			\
	types.coffee			\
	consts.coffee			\
	exitable-scope.coffee		\
	runtime.coffee			\
	terminal.coffee

DESTDIR = generated

JS_FILES=$(COFFEE_FILES:%.coffee=$(DESTDIR)/%.js)

all-local:: $(JS_FILES)

clean-local::
	rm -f $(JS_FILES)

$(DESTDIR)/%.js: %.coffee
	coffee -o $(DESTDIR) -c $<

include $(TOP)/build/build.mk
