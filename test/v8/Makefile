TOPDIR=$(shell pwd)/../..
NODE_PATH=$(TOPDIR)/node-llvm/build/Release:$(TOPDIR)/lib/coffee:$(TOPDIR)/lib:$(TOPDIR)/esprima:$(TOPDIR)/escodegen:$(TOPDIR)/estraverse

TESTS = crypto.js        \
	deltablue.js     \
	earley-boyer.js  \
	navier-stokes.js \
	raytrace.js      \
	regexp.js        \
	richards.js      \
	splay.js

TEST_EXES=$(TESTS:%.js=%.js.exe)

all: $(TEST_EXES)
clean:
	rm -f $(TEST_EXES)

$(TEST_EXES):%.js.exe: %.js base.js run.js $(TOPDIR)/runtime/libecho.a
	@rm -f base.js.exe
	NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs -s base.js $< run.js
	@mv base.js.exe $@

