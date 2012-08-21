TOPDIR=$(shell pwd)/..
NODE_PATH=$(TOPDIR)/node-llvm/build/default:$(TOPDIR)/lib/coffee:$(TOPDIR)/lib:$(TOPDIR)/esprima:$(TOPDIR)/escodegen

TESTS=$(wildcard *[0-9].js)

check: check-llvm run-tests

check-llvm:
	@coffee -c terminal.coffee
	@coffee -c echo-assert.coffee
	@NODE_PATH=$(NODE_PATH):. coffee llvm-test.coffee

run-tests: $(patsubst %.js,test-%,$(TESTS))

test-%: echo-% %.ejs-out %.node-out
	-@test_js=`echo $@|sed -e s,test-,,`; \
	if cmp -s $$test_js.ejs-out $$test_js.node-out ; then \
		echo "\033[32mSUCCESS\033[0m"; \
	else \
		echo "\033[31mFAILURE\033[0m"; \
		diff $$test_js.ejs-out $$test_js.node-out > $$test_js.diff; \
	fi

echo-%:
	-@test_js=`echo $@|sed -e s,echo-,,`; \
	/bin/echo -n "$$test_js:   "

%.ejs-out: %.js.exe
	-@EXE=`echo $<`; ./$$EXE > $@

%.node-out: %.js
	@node $< > $@

%.js.exe: %.js
	-@NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs $<

v8-%.js.exe: v8/%.js
	NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs $<

run-node:
	NODE_PATH=$(NODE_PATH) node

set.js: ../lib/set.coffee
	@NODE_PATH=$(NODE_PATH):. coffee -b -c -o . ../lib/set.coffee

self-test: set.js.exe
	./set.js.exe

clean:
	rm -f *.o *.js.exe terminal.js echo-assert.js *.diff

