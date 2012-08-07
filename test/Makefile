TOPDIR=$(shell pwd)/..
NODE_PATH=$(TOPDIR)/node-llvm/build/default:$(TOPDIR)/lib/coffee:$(TOPDIR)/lib:$(TOPDIR)/esprima:$(TOPDIR)/escodegen

check: test-foo test-hello
	./hello.js.exe
	./foo.js.exe

test-foo:
	@echo [ejs] foo.js.exe
	@NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs foo.js

test-hello:
	@echo [ejs] hello.js.exe
	@NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs hello.js

test-hello2:
	@echo [ejs] hello2.js.exe
	@NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs hello2.js

test-set:
	NODE_PATH=$(NODE_PATH) coffee ../lib/set.coffee

run-node:
	NODE_PATH=$(NODE_PATH) node

clean:
	rm -f *.o *.exe

