COFFEE_FILES=				\
	nodevisitor.coffee		\
	compiler.coffee			\
	debug.coffee			\
	echo-util.coffee		\
	closure-conversion.coffee	\
	set.coffee			\
	types.coffee			\
	exitable-scope.coffee

DESTDIR = coffee

JS_FILES=$(COFFEE_FILES:%.coffee=$(DESTDIR)/%.js)

all: $(JS_FILES)


$(DESTDIR)/%.js: %.coffee
	coffee -o $(DESTDIR) -c $<

