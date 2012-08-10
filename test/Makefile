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

run-node:
	NODE_PATH=$(NODE_PATH) node

clean:
	rm -f *.o *.js.exe

