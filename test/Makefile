TOPDIR=$(shell pwd)/..
NODE_PATH=$(TOPDIR)/node-llvm/build/default:$(TOPDIR)/lib/coffee:$(TOPDIR)/lib:$(TOPDIR)/esprima:$(TOPDIR)/escodegen

HELLOS=$(patsubst %.js,test-%,$(wildcard hello*.js))
CONDS=$(patsubst %.js,test-%,$(wildcard cond*.js))

check: check-llvm $(HELLOS) $(CONDS)

check-llvm:
	@coffee -c terminal.coffee
	@coffee -c echo-assert.coffee
	@NODE_PATH=$(NODE_PATH):. coffee llvm-test.coffee

test-%: %.js.exe
	@EXE=`echo $<`; ./$$EXE

%.js.exe: %.js
	@echo [ejs] $<
	@NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs $<

v8-%.js.exe: v8/%.js
	@echo [ejs] $<
	@NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs $<

run-node:
	NODE_PATH=$(NODE_PATH) node

set.js: ../lib/set.coffee
	@NODE_PATH=$(NODE_PATH):. coffee -b -c -o . ../lib/set.coffee

self-test: set.js.exe
	./set.js.exe

clean:
	rm -f *.o *.js.exe terminal.js echo-assert.js

