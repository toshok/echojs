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

TEST_EXES=$(TESTS:%.js=%-test.js.exe)

all: $(TEST_EXES)
clean:
	rm -f $(TEST_EXES)

$(TEST_EXES):%-test.js.exe: %-test.js $(TOPDIR)/runtime/libecho.a
	NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs --srcdir $<

.INTERMEDIATE: $(TESTS:%.js=%-test.js)

%-test.js: %.js run.js base.js
	cat $^ > $@