TOPDIR=$(shell pwd)/..
NODE_PATH=$(TOPDIR)/node-llvm/build/default:$(TOPDIR)/lib/coffee:$(TOPDIR)/lib:$(TOPDIR)/esprima:$(TOPDIR)/escodegen

HELLOS=$(patsubst %.js,test-%,$(wildcard hello*.js))

check: check-llvm $(HELLOS)

check-llvm:
	@coffee -c terminal.coffee
	@coffee -c echo-assert.coffee
	@NODE_PATH=$(NODE_PATH):. coffee llvm-test.coffee

test-foo:
	@echo [ejs] foo.js.exe
	@NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs foo.js

test-%: %.js.exe
	@EXE=`echo $<`; ./$$EXE

%.js.exe: %.js
	@echo [ejs] $<
	@NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs $<

test-set:
	NODE_PATH=$(NODE_PATH) coffee ../lib/set.coffee

run-node:
	NODE_PATH=$(NODE_PATH) node

clean:
	rm -f *.o *.js.exe

