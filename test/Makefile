TOPDIR=$(shell pwd)/..
NODE_PATH=$(TOPDIR)/node-llvm/build/default:$(TOPDIR)/esprima:$(TOPDIR)/lib

check: test-foo test-hello
	./hello.js.exe
	./foo.js.exe

test-foo:
	NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs foo.js

test-hello:
	NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs hello.js


run-node:
	NODE_PATH=$(NODE_PATH) node

clean:
	rm -f *.o *.exe

